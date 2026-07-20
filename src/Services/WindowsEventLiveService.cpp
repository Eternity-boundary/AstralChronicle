// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventLiveService.h"

#include <winevt.h>

#include <cstddef>
#include <string_view>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace AstralChronicle::services
{
    namespace
    {
        [[nodiscard]] std::wstring XmlValue(std::wstring const& xml, std::wstring_view tag)
        {
            auto const open = xml.find(L"<" + std::wstring{ tag } + L">");
            if (open == std::wstring::npos) return {};
            auto const start = open + tag.size() + 2;
            auto const end = xml.find(L"</" + std::wstring{ tag } + L">", start);
            return end == std::wstring::npos ? std::wstring{} : xml.substr(start, end - start);
        }
    }

    WindowsEventLiveService::~WindowsEventLiveService()
    {
        Stop();
    }

    bool WindowsEventLiveService::Start(
        std::wstring_view channel,
        std::wstring_view query,
        std::uint32_t const queueLimit)
    {
        Stop();
        {
            std::scoped_lock lock{ m_mutex };
            m_channel = channel.empty() ? L"System" : std::wstring{ channel };
            m_query = query.empty() ? L"*" : std::wstring{ query };
            m_queueLimit = std::max<std::uint32_t>(queueLimit, 64);
            m_droppedCount = 0;
            m_errorCode = 0;
            m_events.clear();
            m_totalReceived = 0;
            m_criticalCount = 0;
            m_errorCount = 0;
            m_warningCount = 0;
            m_startedAt = std::chrono::steady_clock::now();
            m_state = LiveState::Starting;
        }
        unique_evt_handle subscription{ EvtSubscribe(
            nullptr,
            nullptr,
            m_channel.c_str(),
            m_query.c_str(),
            nullptr,
            this,
            &WindowsEventLiveService::NotificationCallback,
            EvtSubscribeToFutureEvents) };
        std::scoped_lock lock{ m_mutex };
        if (!subscription)
        {
            m_errorCode = GetLastError();
            m_state = LiveState::Error;
            return false;
        }
        m_subscription = std::move(subscription);
        m_state = LiveState::Running;
        return true;
    }

    void WindowsEventLiveService::Pause() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        if (m_state == LiveState::Running || m_state == LiveState::EventsLost) m_state = LiveState::Paused;
    }

    void WindowsEventLiveService::Resume() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        if (m_state == LiveState::Paused) m_state = LiveState::Running;
    }

    void WindowsEventLiveService::Stop() noexcept
    {
        {
            std::scoped_lock lock{ m_mutex };
            if (m_subscription) m_state = LiveState::Stopping;
        }
        m_subscription.reset();
        std::scoped_lock lock{ m_mutex };
        m_state = LiveState::Stopped;
    }

    void WindowsEventLiveService::Clear() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        m_events.clear();
        m_droppedCount = 0;
    }

    LiveBatch WindowsEventLiveService::TakeBatch(std::uint32_t const maximumEvents)
    {
        std::scoped_lock lock{ m_mutex };
        LiveBatch result;
        result.State = m_state;
        result.ErrorCode = m_errorCode;
        result.DroppedCount = m_droppedCount;
        result.QueueDepth = static_cast<std::uint32_t>(m_events.size());
        result.TotalReceived = m_totalReceived;
        result.CriticalCount = m_criticalCount;
        result.ErrorCount = m_errorCount;
        result.WarningCount = m_warningCount;
        result.Duration = m_startedAt.time_since_epoch().count() == 0
            ? std::chrono::milliseconds{}
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startedAt);
        m_droppedCount = 0;
        auto const limit = maximumEvents;
        if (limit == 0)
        {
            return result;
        }
        while (!m_events.empty() && result.Events.size() < limit)
        {
            result.Events.emplace_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        return result;
    }

    DWORD WINAPI WindowsEventLiveService::NotificationCallback(
        EVT_SUBSCRIBE_NOTIFY_ACTION const action,
        PVOID const userContext,
        EVT_HANDLE const event)
    {
        if (userContext)
        {
            static_cast<WindowsEventLiveService*>(userContext)->HandleNotification(action, event);
        }
        return 0;
    }

    void WindowsEventLiveService::HandleNotification(
        EVT_SUBSCRIBE_NOTIFY_ACTION const action,
        EVT_HANDLE const event) noexcept
    {
        if (action == EvtSubscribeActionError)
        {
            std::scoped_lock lock{ m_mutex };
            m_errorCode = event ? static_cast<std::uint32_t>(reinterpret_cast<ULONG_PTR>(event)) : ERROR_EVT_QUERY_RESULT_STALE;
            m_state = LiveState::Error;
            return;
        }
        if (action != EvtSubscribeActionDeliver || !event)
        {
            return;
        }
        try
        {
            auto rendered = RenderEvent(event);
            if (rendered.empty()) return;
            std::scoped_lock lock{ m_mutex };
            if (m_state != LiveState::Running && m_state != LiveState::Paused && m_state != LiveState::EventsLost)
            {
                return;
            }
            ++m_totalReceived;
            auto const level = XmlValue(rendered, L"Level");
            if (level == L"1") ++m_criticalCount;
            else if (level == L"2") ++m_errorCount;
            else if (level == L"3") ++m_warningCount;
            if (m_events.size() >= m_queueLimit)
            {
                m_events.pop_front();
                ++m_droppedCount;
                m_state = LiveState::EventsLost;
            }
            m_events.emplace_back(std::move(rendered));
        }
        catch (...)
        {
            std::scoped_lock lock{ m_mutex };
            m_errorCode = ERROR_OUTOFMEMORY;
            m_state = LiveState::Error;
        }
    }

    std::wstring WindowsEventLiveService::RenderEvent(EVT_HANDLE const event) const
    {
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &bufferBytes, &propertyCount) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }
        std::vector<std::byte> buffer(bufferBytes);
        if (!EvtRender(nullptr, event, EvtRenderEventXml, bufferBytes, buffer.data(), &bufferBytes, &propertyCount))
        {
            return {};
        }
        return static_cast<wchar_t const*>(static_cast<void const*>(buffer.data()));
    }
}
