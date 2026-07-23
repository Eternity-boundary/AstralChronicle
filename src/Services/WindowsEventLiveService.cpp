// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventLiveService.h"

#include <winevt.h>

#include <array>
#include <cstddef>
#include <new>
#include <string_view>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace AstralChronicle::services
{
    namespace
    {
        constexpr DWORD EventBatchSize = 32;

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
        std::scoped_lock workerLock{ m_workerMutex };
        StopWorkerLocked();

        std::wstring channelText{ channel.empty() ? L"System" : channel };
        std::wstring queryText{ query.empty() ? L"*" : query };
        {
            std::scoped_lock lock{ m_mutex };
            m_queueLimit = std::max<std::uint32_t>(queueLimit, 64);
            m_droppedSinceLastBatch = 0;
            m_errorCode = 0;
            m_events.clear();
            m_totalReceived = 0;
            m_criticalCount = 0;
            m_errorCount = 0;
            m_warningCount = 0;
            m_startedAt = std::chrono::steady_clock::now();
            m_state = LiveState::Starting;
            m_pauseRequested = false;
        }

        m_stopEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!m_stopEvent)
        {
            SetWorkerError(GetLastError());
            return false;
        }

        std::promise<WorkerStartResult> startup;
        auto startupResult = startup.get_future();
        auto const stopEvent = m_stopEvent.get();
        try
        {
            m_worker = std::jthread(
                [this,
                 stopEvent,
                 channel = std::move(channelText),
                 query = std::move(queryText),
                 startup = std::move(startup)](std::stop_token const stopToken) mutable
                {
                    RunWorker(
                        stopToken,
                        stopEvent,
                        std::move(channel),
                        std::move(query),
                        std::move(startup));
                });
        }
        catch (...)
        {
            m_stopEvent.reset();
            SetWorkerError(ERROR_NOT_ENOUGH_MEMORY);
            return false;
        }

        WorkerStartResult result;
        try
        {
            result = startupResult.get();
        }
        catch (...)
        {
            result.ErrorCode = ERROR_GEN_FAILURE;
        }

        if (!result.Success)
        {
            if (m_worker.joinable())
            {
                m_worker.request_stop();
                SetEvent(m_stopEvent.get());
                m_worker.join();
            }
            m_stopEvent.reset();
            SetWorkerError(result.ErrorCode == 0 ? ERROR_GEN_FAILURE : result.ErrorCode);
        }
        return result.Success;
    }

    void WindowsEventLiveService::Pause() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        if (m_state == LiveState::Running || m_state == LiveState::EventsLost)
        {
            m_pauseRequested = true;
            m_state = LiveState::Paused;
        }
    }

    void WindowsEventLiveService::Resume() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        if (m_state == LiveState::Paused)
        {
            m_pauseRequested = false;
            m_state = LiveState::Running;
        }
    }

    void WindowsEventLiveService::Stop() noexcept
    {
        std::scoped_lock workerLock{ m_workerMutex };
        StopWorkerLocked();
    }

    void WindowsEventLiveService::StopWorkerLocked() noexcept
    {
        if (m_worker.joinable())
        {
            {
                std::scoped_lock lock{ m_mutex };
                m_state = LiveState::Stopping;
            }
            m_worker.request_stop();
            if (m_stopEvent)
            {
                SetEvent(m_stopEvent.get());
            }
            m_worker.join();
        }
        m_stopEvent.reset();

        std::scoped_lock lock{ m_mutex };
        m_pauseRequested = false;
        m_state = LiveState::Stopped;
    }

    void WindowsEventLiveService::Clear() noexcept
    {
        std::scoped_lock lock{ m_mutex };
        m_events.clear();
        m_droppedSinceLastBatch = 0;
        if (m_state == LiveState::EventsLost)
        {
            m_state = m_pauseRequested ? LiveState::Paused : LiveState::Running;
        }
    }

    LiveBatch WindowsEventLiveService::TakeBatch(std::uint32_t const maximumEvents)
    {
        std::scoped_lock lock{ m_mutex };
        LiveBatch result;
        result.State = m_state;
        result.ErrorCode = m_errorCode;
        result.DroppedCount = m_droppedSinceLastBatch;
        result.QueueDepth = static_cast<std::uint32_t>(m_events.size());
        result.TotalReceived = m_totalReceived;
        result.CriticalCount = m_criticalCount;
        result.ErrorCount = m_errorCount;
        result.WarningCount = m_warningCount;
        result.Duration = m_startedAt.time_since_epoch().count() == 0
            ? std::chrono::milliseconds{}
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startedAt);

        m_droppedSinceLastBatch = 0;
        if (m_state == LiveState::EventsLost)
        {
            m_state = m_pauseRequested ? LiveState::Paused : LiveState::Running;
        }

        while (!m_events.empty() && result.Events.size() < maximumEvents)
        {
            result.Events.emplace_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        result.QueueDepth = static_cast<std::uint32_t>(m_events.size());
        return result;
    }

    void WindowsEventLiveService::RunWorker(
        std::stop_token const stopToken,
        HANDLE const stopEvent,
        std::wstring channel,
        std::wstring query,
        std::promise<WorkerStartResult> startup)
    {
        bool startupReported{};
        auto reportStartup = [&startup, &startupReported](WorkerStartResult const result) noexcept
        {
            if (startupReported) return;
            startupReported = true;
            try
            {
                startup.set_value(result);
            }
            catch (...)
            {
            }
        };

        try
        {
            wil::unique_handle subscriptionEvent;
            subscriptionEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!subscriptionEvent)
            {
                auto const error = GetLastError();
                SetWorkerError(error);
                reportStartup({ false, error });
                return;
            }

            unique_evt_handle subscription{ EvtSubscribe(
                nullptr,
                subscriptionEvent.get(),
                channel.c_str(),
                query.c_str(),
                nullptr,
                nullptr,
                nullptr,
                EvtSubscribeToFutureEvents) };
            if (!subscription)
            {
                auto const error = GetLastError();
                SetWorkerError(error);
                reportStartup({ false, error });
                return;
            }

            {
                std::scoped_lock lock{ m_mutex };
                m_state = m_pauseRequested ? LiveState::Paused : LiveState::Running;
            }
            reportStartup({ true, ERROR_SUCCESS });

            HANDLE waitHandles[]{ stopEvent, subscriptionEvent.get() };
            while (!stopToken.stop_requested())
            {
                auto const waitResult = WaitForMultipleObjects(
                    ARRAYSIZE(waitHandles),
                    waitHandles,
                    FALSE,
                    INFINITE);
                if (waitResult == WAIT_OBJECT_0)
                {
                    break;
                }
                if (waitResult != WAIT_OBJECT_0 + 1)
                {
                    SetWorkerError(waitResult == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE);
                    return;
                }
                while (!stopToken.stop_requested())
                {
                    std::array<EVT_HANDLE, EventBatchSize> rawEvents{};
                    DWORD returned{};
                    auto const succeeded = EvtNext(
                        subscription.get(),
                        static_cast<DWORD>(rawEvents.size()),
                        rawEvents.data(),
                        0,
                        0,
                        &returned);
                    auto const error = succeeded ? ERROR_SUCCESS : GetLastError();

                    std::array<unique_evt_handle, EventBatchSize> events;
                    for (std::size_t index{}; index < rawEvents.size(); ++index)
                    {
                        events[index].reset(rawEvents[index]);
                    }

                    if (!succeeded)
                    {
                        if (error == ERROR_NO_MORE_ITEMS || error == ERROR_TIMEOUT)
                        {
                            if (!ResetEvent(subscriptionEvent.get()))
                            {
                                SetWorkerError(GetLastError());
                                return;
                            }
                            break;
                        }
                        SetWorkerError(error);
                        return;
                    }

                    for (DWORD index{}; index < returned; ++index)
                    {
                        ProcessEvent(events[index].get());
                    }
                }
            }
        }
        catch (std::bad_alloc const&)
        {
            SetWorkerError(ERROR_OUTOFMEMORY);
            reportStartup({ false, ERROR_OUTOFMEMORY });
        }
        catch (...)
        {
            SetWorkerError(ERROR_GEN_FAILURE);
            reportStartup({ false, ERROR_GEN_FAILURE });
        }
    }

    void WindowsEventLiveService::ProcessEvent(EVT_HANDLE const event)
    {
        auto rendered = RenderEvent(event);
        if (rendered.empty()) return;

        std::scoped_lock lock{ m_mutex };
        if (m_state != LiveState::Running &&
            m_state != LiveState::Paused &&
            m_state != LiveState::EventsLost)
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
            ++m_droppedSinceLastBatch;
            m_state = LiveState::EventsLost;
        }
        m_events.emplace_back(std::move(rendered));
    }

    void WindowsEventLiveService::SetWorkerError(std::uint32_t const errorCode) noexcept
    {
        try
        {
            std::scoped_lock lock{ m_mutex };
            if (m_state == LiveState::Stopping)
            {
                return;
            }
            m_errorCode = errorCode == 0 ? ERROR_GEN_FAILURE : errorCode;
            m_state = LiveState::Error;
        }
        catch (...)
        {
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
