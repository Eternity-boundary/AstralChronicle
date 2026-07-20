// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsRemoteEventService.h"

#include <winevt.h>

#include <vector>
#include <algorithm>
#include <chrono>
#include <ratio>

#pragma comment(lib, "wevtapi.lib")

namespace
{
    constexpr std::uint64_t WindowsEpochOffset100Nanoseconds = 116444736000000000ULL;

    [[nodiscard]] std::chrono::system_clock::time_point TimePointFromWindowsTicks(std::uint64_t ticks)
    {
        if (ticks < WindowsEpochOffset100Nanoseconds) return {};
        using WindowsDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
        return std::chrono::system_clock::time_point{ std::chrono::duration_cast<std::chrono::system_clock::duration>(
            WindowsDuration{ static_cast<std::int64_t>(ticks - WindowsEpochOffset100Nanoseconds) }) };
    }

    [[nodiscard]] AstralChronicle::services::EventQueryStatus MapError(DWORD error)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        switch (error)
        {
        case ERROR_ACCESS_DENIED: return Status::AccessDenied;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_EVT_CHANNEL_NOT_FOUND: return Status::InvalidChannel;
        case ERROR_CANCELLED: return Status::Cancelled;
        default: return Status::Failed;
        }
    }

    [[nodiscard]] AstralChronicle::models::EventRecordSummary RenderRemoteSummary(EVT_HANDLE eventHandle)
    {
        DWORD bytes{};
        DWORD propertyCount{};
        if (!EvtRender(nullptr, eventHandle, EvtRenderEventValues, 0, nullptr, &bytes, &propertyCount) &&
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) return {};
        std::vector<std::byte> buffer(bytes);
        auto values = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtRender(nullptr, eventHandle, EvtRenderEventValues, bytes, values, &bytes, &propertyCount)) return {};
        AstralChronicle::models::EventRecordSummary summary;
        if (propertyCount > EvtSystemProviderName && values[EvtSystemProviderName].StringVal) summary.Provider = values[EvtSystemProviderName].StringVal;
        if (propertyCount > EvtSystemEventID) summary.EventId = values[EvtSystemEventID].UInt16Val;
        if (propertyCount > EvtSystemLevel) summary.Level = values[EvtSystemLevel].ByteVal;
        if (propertyCount > EvtSystemVersion) summary.Version = values[EvtSystemVersion].ByteVal;
        if (propertyCount > EvtSystemOpcode) summary.Opcode = values[EvtSystemOpcode].ByteVal;
        if (propertyCount > EvtSystemKeywords) summary.Keywords = values[EvtSystemKeywords].UInt64Val;
        if (propertyCount > EvtSystemEventRecordId) summary.RecordId = values[EvtSystemEventRecordId].UInt64Val;
        if (propertyCount > EvtSystemTimeCreated) summary.TimeCreated = TimePointFromWindowsTicks(values[EvtSystemTimeCreated].FileTimeVal);
        if (propertyCount > EvtSystemChannel && values[EvtSystemChannel].StringVal) summary.Channel = values[EvtSystemChannel].StringVal;
        if (propertyCount > EvtSystemComputer && values[EvtSystemComputer].StringVal) summary.Computer = values[EvtSystemComputer].StringVal;
        if (propertyCount > EvtSystemProcessID) summary.ProcessId = values[EvtSystemProcessID].UInt32Val;
        if (propertyCount > EvtSystemThreadID) summary.ThreadId = values[EvtSystemThreadID].UInt32Val;
        return summary;
    }
}

namespace AstralChronicle::services
{
    bool WindowsRemoteEventService::Connect(
        std::wstring_view host,
        std::wstring_view domain,
        std::wstring_view user,
        std::wstring_view password)
    {
        Disconnect();
        m_host = host;
        EVT_RPC_LOGIN login{};
        std::wstring hostValue{ host };
        std::wstring domainValue{ domain };
        std::wstring userValue{ user };
        std::wstring passwordValue{ password };
        login.Server = const_cast<wchar_t*>(hostValue.c_str());
        login.Domain = const_cast<wchar_t*>(domainValue.c_str());
        login.User = const_cast<wchar_t*>(userValue.c_str());
        login.Password = const_cast<wchar_t*>(passwordValue.c_str());
        login.Flags = EvtRpcLoginAuthNegotiate;
        m_session.reset(EvtOpenSession(EvtRpcLogin, &login, 0, 0));
        m_lastError = m_session ? 0 : GetLastError();
        return static_cast<bool>(m_session);
    }

    void WindowsRemoteEventService::Disconnect() noexcept
    {
        StopLive();
        m_session.reset();
    }

    bool WindowsRemoteEventService::IsConnected() const noexcept { return static_cast<bool>(m_session); }
    std::uint32_t WindowsRemoteEventService::LastError() const noexcept { return m_lastError; }
    std::wstring_view WindowsRemoteEventService::Host() const noexcept { return m_host; }

    std::vector<models::EventChannelDescriptor> WindowsRemoteEventService::EnumerateChannels() const
    {
        std::vector<models::EventChannelDescriptor> channels;
        if (!m_session)
        {
            return channels;
        }

        unique_evt_handle enumerator{ EvtOpenChannelEnum(m_session.get(), 0) };
        if (!enumerator)
        {
            return channels;
        }

        for (;;)
        {
            DWORD requiredCharacters{};
            if (EvtNextChannelPath(enumerator.get(), 0, nullptr, &requiredCharacters))
            {
                continue;
            }
            auto const error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (error != ERROR_INSUFFICIENT_BUFFER || requiredCharacters == 0)
            {
                break;
            }

            std::vector<wchar_t> path(requiredCharacters);
            if (!EvtNextChannelPath(enumerator.get(), requiredCharacters, path.data(), &requiredCharacters))
            {
                break;
            }

            models::EventChannelDescriptor descriptor;
            descriptor.Path = path.data();
            unique_evt_handle config{ EvtOpenChannelConfig(m_session.get(), descriptor.Path.c_str(), 0) };
            if (!config)
            {
                descriptor.State = GetLastError() == ERROR_ACCESS_DENIED
                    ? models::EventChannelState::AccessDenied
                    : models::EventChannelState::Unavailable;
            }
            else
            {
                EVT_VARIANT enabled{};
                DWORD usedBytes{};
                descriptor.State = EvtGetChannelConfigProperty(
                    config.get(),
                    EvtChannelConfigEnabled,
                    0,
                    sizeof(enabled),
                    &enabled,
                    &usedBytes)
                    ? (enabled.BooleanVal ? models::EventChannelState::Available : models::EventChannelState::Disabled)
                    : models::EventChannelState::Unavailable;
            }
            channels.emplace_back(std::move(descriptor));
        }
        return channels;
    }

    bool WindowsRemoteEventService::StartLive(std::wstring_view channel, std::wstring_view query, std::uint32_t const queueLimit)
    {
        StopLive();
        if (!m_session) return false;
        std::wstring channelText{ channel };
        std::wstring queryText{ query.empty() ? L"*" : query };
        {
            std::scoped_lock lock{ m_liveMutex };
            m_liveQueueLimit = std::max<std::uint32_t>(1, queueLimit);
            m_liveDroppedCount = 0;
            m_liveErrorCode = 0;
            m_liveTotalReceived = 0;
            m_liveEvents.clear();
            m_liveStartedAt = std::chrono::steady_clock::now();
            m_liveState = LiveState::Starting;
        }
        m_liveSubscription.reset(EvtSubscribe(m_session.get(), nullptr, channelText.c_str(), queryText.c_str(), nullptr, this, NotificationCallback, EvtSubscribeToFutureEvents));
        std::scoped_lock lock{ m_liveMutex };
        if (!m_liveSubscription)
        {
            m_liveErrorCode = GetLastError();
            m_liveState = LiveState::Error;
            return false;
        }
        m_liveState = LiveState::Running;
        return true;
    }

    void WindowsRemoteEventService::StopLive() noexcept
    {
        {
            std::scoped_lock lock{ m_liveMutex };
            m_liveState = LiveState::Stopping;
        }
        m_liveSubscription.reset();
        std::scoped_lock lock{ m_liveMutex };
        m_liveState = LiveState::Stopped;
    }

    LiveBatch WindowsRemoteEventService::TakeLiveBatch(std::uint32_t const maximumEvents)
    {
        LiveBatch batch;
        std::scoped_lock lock{ m_liveMutex };
        batch.State = m_liveState;
        batch.ErrorCode = m_liveErrorCode;
        batch.DroppedCount = m_liveDroppedCount;
        batch.QueueDepth = static_cast<std::uint32_t>(m_liveEvents.size());
        batch.TotalReceived = m_liveTotalReceived;
        batch.Duration = m_liveStartedAt.time_since_epoch().count() == 0
            ? std::chrono::milliseconds{}
            : std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_liveStartedAt);
        auto const count = std::min<std::size_t>(maximumEvents, m_liveEvents.size());
        batch.Events.reserve(count);
        for (std::size_t index{}; index < count; ++index)
        {
            batch.Events.emplace_back(std::move(m_liveEvents.front()));
            m_liveEvents.pop_front();
        }
        batch.QueueDepth = static_cast<std::uint32_t>(m_liveEvents.size());
        return batch;
    }

    DWORD WINAPI WindowsRemoteEventService::NotificationCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID userContext, EVT_HANDLE event)
    {
        auto const service = static_cast<WindowsRemoteEventService*>(userContext);
        if (service) service->HandleNotification(action, event);
        return ERROR_SUCCESS;
    }

    void WindowsRemoteEventService::HandleNotification(EVT_SUBSCRIBE_NOTIFY_ACTION const action, EVT_HANDLE const event) noexcept
    {
        std::scoped_lock lock{ m_liveMutex };
        if (action == EvtSubscribeActionError)
        {
            m_liveErrorCode = static_cast<std::uint32_t>(reinterpret_cast<ULONG_PTR>(event));
            m_liveState = LiveState::Error;
            return;
        }
        if (action != EvtSubscribeActionDeliver || !event) return;
        auto xml = RenderEvent(event);
        ++m_liveTotalReceived;
        if (m_liveEvents.size() >= m_liveQueueLimit)
        {
            m_liveEvents.pop_front();
            ++m_liveDroppedCount;
            m_liveState = LiveState::EventsLost;
        }
        if (!xml.empty()) m_liveEvents.emplace_back(std::move(xml));
    }

    std::wstring WindowsRemoteEventService::RenderEvent(EVT_HANDLE const event) const
    {
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (!EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &bufferBytes, &propertyCount) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) return {};
        std::vector<wchar_t> buffer(bufferBytes / sizeof(wchar_t) + 1);
        if (!EvtRender(nullptr, event, EvtRenderEventXml, bufferBytes, buffer.data(), &bufferBytes, &propertyCount)) return {};
        return buffer.data();
    }

    EventQueryResult WindowsRemoteEventService::QueryPage(
        std::wstring_view channel,
        std::wstring_view query,
        std::uint32_t const maximumRecords,
        bool const reverseDirection,
        QueryCancellation const& cancellation) const
    {
        EventQueryResult result;
        if (!m_session)
        {
            result.Status = EventQueryStatus::ServiceUnavailable;
            result.ErrorCode = ERROR_INVALID_HANDLE;
            return result;
        }
        if (maximumRecords == 0)
        {
            result.Status = EventQueryStatus::NoEvents;
            return result;
        }
        DWORD flags = EvtQueryChannelPath;
        if (reverseDirection) flags |= EvtQueryReverseDirection;
        std::wstring channelText{ channel };
        std::wstring queryText{ query.empty() ? L"*" : query };
        unique_evt_handle queryHandle{ EvtQuery(m_session.get(), channelText.c_str(), queryText.c_str(), flags) };
        if (!queryHandle)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapError(result.ErrorCode);
            return result;
        }
        std::vector<EVT_HANDLE> rawEvents(32);
        while (result.Events.size() < maximumRecords)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }
            DWORD returned{};
            if (!EvtNext(queryHandle.get(), static_cast<DWORD>(rawEvents.size()), rawEvents.data(), INFINITE, 0, &returned))
            {
                auto const error = GetLastError();
                if (error == ERROR_NO_MORE_ITEMS) break;
                result.ErrorCode = error;
                result.Status = MapError(error);
                return result;
            }
            for (DWORD index{}; index < returned; ++index)
            {
                unique_evt_handle event{ rawEvents[index] };
                rawEvents[index] = nullptr;
                if (result.Events.size() < maximumRecords)
                {
                    result.Events.emplace_back(RenderRemoteSummary(event.get()));
                }
            }
        }
        result.Status = result.Events.empty() ? EventQueryStatus::NoEvents : EventQueryStatus::Succeeded;
        return result;
    }
}
