// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsRemoteEventService.h"
#include "EvtVariantReader.h"

#include <winevt.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <ratio>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace
{
    constexpr std::uint64_t WindowsEpochOffset100Nanoseconds = 116444736000000000ULL;
    constexpr DWORD EventBatchSize = 32;
    constexpr DWORD QueryPollMilliseconds = 200;

    class secure_password final
    {
    public:
        explicit secure_password(std::wstring_view const value) : m_value(value) {}
        ~secure_password() { clear(); }

        secure_password(secure_password const&) = delete;
        secure_password& operator=(secure_password const&) = delete;

        [[nodiscard]] wchar_t* data_or_null() noexcept
        {
            return m_value.empty() ? nullptr : m_value.data();
        }

        void clear() noexcept
        {
            if (!m_value.empty())
            {
                SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
                m_value.clear();
            }
        }

    private:
        std::wstring m_value;
    };

    [[nodiscard]] std::chrono::system_clock::time_point TimePointFromWindowsTicks(std::uint64_t const ticks)
    {
        if (ticks < WindowsEpochOffset100Nanoseconds) return {};
        using WindowsDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
        auto const unixTicks = ticks - WindowsEpochOffset100Nanoseconds;
        if (unixTicks > static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)()))
        {
            return {};
        }
        return std::chrono::system_clock::time_point{ std::chrono::duration_cast<std::chrono::system_clock::duration>(
            WindowsDuration{ static_cast<std::int64_t>(unixTicks) }) };
    }

    [[nodiscard]] AstralChronicle::services::EventQueryStatus MapError(DWORD const error)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        switch (error)
        {
        case ERROR_ACCESS_DENIED: return Status::AccessDenied;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_EVT_CHANNEL_NOT_FOUND: return Status::InvalidChannel;
        case ERROR_SERVICE_NOT_ACTIVE: return Status::ServiceUnavailable;
        case ERROR_CANCELLED: return Status::Cancelled;
        default: return Status::Failed;
        }
    }

    [[nodiscard]] std::optional<AstralChronicle::models::EventRecordSummary> RenderRemoteSummary(
        EVT_HANDLE const eventHandle,
        EVT_HANDLE const renderContext,
        DWORD& errorCode)
    {
        errorCode = ERROR_SUCCESS;
        if (!renderContext)
        {
            errorCode = ERROR_INVALID_HANDLE;
            return std::nullopt;
        }

        DWORD bytes{};
        DWORD propertyCount{};
        if (EvtRender(renderContext, eventHandle, EvtRenderEventValues, 0, nullptr, &bytes, &propertyCount))
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }
        errorCode = GetLastError();
        if (errorCode != ERROR_INSUFFICIENT_BUFFER || bytes == 0)
        {
            return std::nullopt;
        }
        std::vector<EVT_VARIANT> buffer(
            (static_cast<std::size_t>(bytes) + sizeof(EVT_VARIANT) - 1) /
            sizeof(EVT_VARIANT));
        auto const values = buffer.data();
        if (!EvtRender(renderContext, eventHandle, EvtRenderEventValues, bytes, values, &bytes, &propertyCount))
        {
            errorCode = GetLastError();
            return std::nullopt;
        }

        using AstralChronicle::services::details::ReadEventId;
        using AstralChronicle::services::details::ReadKeywords;
        using AstralChronicle::services::details::TryGetSystemVariant;
        auto const provider = TryGetSystemVariant(values, propertyCount, EvtSystemProviderName, EvtVarTypeString);
        auto const eventId = ReadEventId(values, propertyCount);
        auto const recordId = TryGetSystemVariant(values, propertyCount, EvtSystemEventRecordId, EvtVarTypeUInt64);
        if (!provider || !provider->StringVal || !eventId || !recordId)
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }

        AstralChronicle::models::EventRecordSummary summary;
        summary.Provider = provider->StringVal;
        summary.EventId = *eventId;
        summary.RecordId = recordId->UInt64Val;
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemLevel, EvtVarTypeByte)) summary.Level = value->ByteVal;
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemVersion, EvtVarTypeByte)) summary.Version = value->ByteVal;
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemOpcode, EvtVarTypeByte)) summary.Opcode = value->ByteVal;
        if (auto const keywords = ReadKeywords(values, propertyCount))
        {
            summary.Keywords = *keywords;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemTimeCreated, EvtVarTypeFileTime))
        {
            summary.TimeCreated = TimePointFromWindowsTicks(value->FileTimeVal);
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemChannel, EvtVarTypeString);
            value && value->StringVal)
        {
            summary.Channel = value->StringVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemComputer, EvtVarTypeString);
            value && value->StringVal)
        {
            summary.Computer = value->StringVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemProcessID, EvtVarTypeUInt32)) summary.ProcessId = value->UInt32Val;
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemThreadID, EvtVarTypeUInt32)) summary.ThreadId = value->UInt32Val;
        errorCode = ERROR_SUCCESS;
        return summary;
    }
}

namespace AstralChronicle::services
{
    WindowsRemoteEventService::~WindowsRemoteEventService()
    {
        Disconnect();
    }

    WindowsRemoteEventService::SessionOpenResult WindowsRemoteEventService::OpenValidatedSession(
        std::wstring_view const host,
        std::wstring_view const domain,
        std::wstring_view const user,
        std::wstring_view const password)
    {
        if (host.empty())
        {
            return { {}, ERROR_INVALID_PARAMETER };
        }

        std::wstring hostValue{ host };
        std::wstring domainValue{ domain };
        std::wstring userValue{ user };
        secure_password passwordValue{ password };
        EVT_RPC_LOGIN login{};
        login.Server = hostValue.data();
        login.Domain = domainValue.empty() ? nullptr : domainValue.data();
        login.User = userValue.empty() ? nullptr : userValue.data();
        login.Password = passwordValue.data_or_null();
        login.Flags = EvtRpcLoginAuthNegotiate;

        unique_evt_handle session{ EvtOpenSession(EvtRpcLogin, &login, 0, 0) };
        auto const openError = session ? ERROR_SUCCESS : GetLastError();
        passwordValue.clear();
        SecureZeroMemory(&login, sizeof(login));
        if (!session)
        {
            return { {}, openError };
        }

        auto const validationError = ValidateSession(session.get());
        if (validationError != ERROR_SUCCESS)
        {
            return { {}, validationError };
        }
        return { std::move(session), ERROR_SUCCESS };
    }

    std::uint32_t WindowsRemoteEventService::ValidateSession(EVT_HANDLE const session)
    {
        unique_evt_handle enumerator{ EvtOpenChannelEnum(session, 0) };
        if (!enumerator)
        {
            return GetLastError();
        }

        DWORD requiredCharacters{};
        if (EvtNextChannelPath(enumerator.get(), 0, nullptr, &requiredCharacters))
        {
            return ERROR_SUCCESS;
        }
        auto const error = GetLastError();
        if (error == ERROR_NO_MORE_ITEMS ||
            (error == ERROR_INSUFFICIENT_BUFFER && requiredCharacters != 0))
        {
            return ERROR_SUCCESS;
        }
        return error;
    }

    bool WindowsRemoteEventService::Connect(
        std::wstring_view const host,
        std::wstring_view const domain,
        std::wstring_view const user,
        std::wstring_view const password)
    {
        std::uint64_t generation{};
        {
            std::scoped_lock lock{ m_stateMutex };
            generation = ++m_generation;
            m_session.reset();
            m_lastError = ERROR_SUCCESS;
        }
        StopLive();

        SessionOpenResult opened;
        try
        {
            opened = OpenValidatedSession(host, domain, user, password);
        }
        catch (std::bad_alloc const&)
        {
            opened.ErrorCode = ERROR_OUTOFMEMORY;
        }
        catch (...)
        {
            opened.ErrorCode = ERROR_GEN_FAILURE;
        }

        std::shared_ptr<SessionState> candidate;
        if (opened.Session)
        {
            try
            {
                candidate = std::make_shared<SessionState>(
                    std::move(opened.Session),
                    std::wstring{ host },
                    generation);
            }
            catch (std::bad_alloc const&)
            {
                opened.ErrorCode = ERROR_OUTOFMEMORY;
            }
            catch (...)
            {
                opened.ErrorCode = ERROR_GEN_FAILURE;
            }
        }

        std::scoped_lock lock{ m_stateMutex };
        if (generation != m_generation)
        {
            return false;
        }
        m_lastError = candidate ? ERROR_SUCCESS :
            (opened.ErrorCode == ERROR_SUCCESS ? ERROR_GEN_FAILURE : opened.ErrorCode);
        m_session = std::move(candidate);
        return static_cast<bool>(m_session);
    }

    RemoteProbeResult WindowsRemoteEventService::Probe(
        std::wstring_view const host,
        std::wstring_view const domain,
        std::wstring_view const user,
        std::wstring_view const password) const
    {
        try
        {
            auto probe = OpenValidatedSession(host, domain, user, password);
            return { static_cast<bool>(probe.Session), probe.ErrorCode };
        }
        catch (std::bad_alloc const&)
        {
            return { false, ERROR_OUTOFMEMORY };
        }
        catch (...)
        {
            return { false, ERROR_GEN_FAILURE };
        }
    }

    void WindowsRemoteEventService::Disconnect() noexcept
    {
        {
            std::scoped_lock lock{ m_stateMutex };
            ++m_generation;
            m_session.reset();
            m_lastError = ERROR_SUCCESS;
        }
        StopLive();
    }

    std::shared_ptr<WindowsRemoteEventService::SessionState> WindowsRemoteEventService::SnapshotSession() const
    {
        std::scoped_lock lock{ m_stateMutex };
        return m_session;
    }

    bool WindowsRemoteEventService::IsConnected() const noexcept
    {
        std::scoped_lock lock{ m_stateMutex };
        return static_cast<bool>(m_session);
    }

    std::uint32_t WindowsRemoteEventService::LastError() const noexcept
    {
        std::scoped_lock lock{ m_stateMutex };
        return m_lastError;
    }

    std::wstring WindowsRemoteEventService::Host() const
    {
        auto const session = SnapshotSession();
        return session ? session->Host : std::wstring{};
    }

    std::vector<models::EventChannelDescriptor> WindowsRemoteEventService::EnumerateChannels() const
    {
        std::vector<models::EventChannelDescriptor> channels;
        auto const session = SnapshotSession();
        if (!session)
        {
            return channels;
        }

        EVT_HANDLE rawEnumerator{};
        {
            std::scoped_lock apiLock{ session->ApiMutex };
            rawEnumerator = EvtOpenChannelEnum(session->Handle.get(), 0);
        }
        unique_evt_handle enumerator{ rawEnumerator };
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
            EVT_HANDLE rawConfig{};
            DWORD configOpenError{};
            {
                std::scoped_lock apiLock{ session->ApiMutex };
                rawConfig = EvtOpenChannelConfig(session->Handle.get(), descriptor.Path.c_str(), 0);
                if (!rawConfig) configOpenError = GetLastError();
            }
            unique_evt_handle config{ rawConfig };
            if (!config)
            {
                descriptor.State = configOpenError == ERROR_ACCESS_DENIED
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

    bool WindowsRemoteEventService::StartLive(
        std::wstring_view const channel,
        std::wstring_view const query,
        std::uint32_t const queueLimit)
    {
        std::scoped_lock workerLock{ m_liveWorkerMutex };
        StopLiveWorkerLocked();
        auto const session = SnapshotSession();
        if (!session)
        {
            {
                std::scoped_lock liveLock{ m_liveMutex };
                m_liveErrorCode = ERROR_INVALID_HANDLE;
                m_liveState = LiveState::Error;
            }
            std::scoped_lock stateLock{ m_stateMutex };
            m_lastError = ERROR_INVALID_HANDLE;
            return false;
        }

        std::wstring channelText{ channel };
        std::wstring queryText{ query.empty() ? L"*" : query };
        {
            std::scoped_lock lock{ m_liveMutex };
            m_liveQueueLimit = std::max<std::uint32_t>(1, queueLimit);
            m_liveDroppedSinceLastBatch = 0;
            m_liveErrorCode = 0;
            m_liveTotalReceived = 0;
            m_liveEvents.clear();
            m_liveStartedAt = std::chrono::steady_clock::now();
            m_liveState = LiveState::Starting;
        }

        m_liveStopEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!m_liveStopEvent)
        {
            auto const error = GetLastError();
            SetLiveError(error);
            std::scoped_lock stateLock{ m_stateMutex };
            if (m_session == session) m_lastError = error;
            return false;
        }

        std::promise<WorkerStartResult> startup;
        auto startupResult = startup.get_future();
        auto const stopEvent = m_liveStopEvent.get();
        try
        {
            m_liveWorker = std::jthread(
                [this,
                 stopEvent,
                 session,
                 channel = std::move(channelText),
                 query = std::move(queryText),
                 startup = std::move(startup)](std::stop_token const stopToken) mutable
                {
                    RunLiveWorker(
                        stopToken,
                        stopEvent,
                        std::move(session),
                        std::move(channel),
                        std::move(query),
                        std::move(startup));
                });
        }
        catch (...)
        {
            m_liveStopEvent.reset();
            SetLiveError(ERROR_NOT_ENOUGH_MEMORY);
            std::scoped_lock stateLock{ m_stateMutex };
            if (m_session == session) m_lastError = ERROR_NOT_ENOUGH_MEMORY;
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
            if (m_liveWorker.joinable())
            {
                m_liveWorker.request_stop();
                SetEvent(m_liveStopEvent.get());
                m_liveWorker.join();
            }
            m_liveStopEvent.reset();
            SetLiveError(result.ErrorCode == 0 ? ERROR_GEN_FAILURE : result.ErrorCode);
            std::scoped_lock stateLock{ m_stateMutex };
            if (m_session == session)
            {
                m_lastError = result.ErrorCode == 0 ? ERROR_GEN_FAILURE : result.ErrorCode;
            }
        }
        return result.Success;
    }

    void WindowsRemoteEventService::StopLive() noexcept
    {
        std::scoped_lock workerLock{ m_liveWorkerMutex };
        StopLiveWorkerLocked();
    }

    void WindowsRemoteEventService::StopLiveWorkerLocked() noexcept
    {
        if (m_liveWorker.joinable())
        {
            {
                std::scoped_lock lock{ m_liveMutex };
                m_liveState = LiveState::Stopping;
            }
            m_liveWorker.request_stop();
            if (m_liveStopEvent)
            {
                SetEvent(m_liveStopEvent.get());
            }
            m_liveWorker.join();
        }
        m_liveStopEvent.reset();

        std::scoped_lock lock{ m_liveMutex };
        m_liveState = LiveState::Stopped;
    }

    LiveBatch WindowsRemoteEventService::TakeLiveBatch(std::uint32_t const maximumEvents)
    {
        LiveBatch batch;
        std::scoped_lock lock{ m_liveMutex };
        batch.State = m_liveState;
        batch.ErrorCode = m_liveErrorCode;
        batch.DroppedCount = m_liveDroppedSinceLastBatch;
        batch.QueueDepth = static_cast<std::uint32_t>(m_liveEvents.size());
        batch.TotalReceived = m_liveTotalReceived;
        batch.Duration = m_liveStartedAt.time_since_epoch().count() == 0
            ? std::chrono::milliseconds{}
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_liveStartedAt);

        m_liveDroppedSinceLastBatch = 0;
        if (m_liveState == LiveState::EventsLost)
        {
            m_liveState = LiveState::Running;
        }

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

    void WindowsRemoteEventService::RunLiveWorker(
        std::stop_token const stopToken,
        HANDLE const stopEvent,
        std::shared_ptr<SessionState> session,
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
                SetLiveError(error);
                reportStartup({ false, error });
                return;
            }

            EVT_HANDLE rawSubscription{};
            DWORD subscribeError{};
            {
                std::scoped_lock apiLock{ session->ApiMutex };
                rawSubscription = EvtSubscribe(
                    session->Handle.get(),
                    subscriptionEvent.get(),
                    channel.c_str(),
                    query.c_str(),
                    nullptr,
                    nullptr,
                    nullptr,
                    EvtSubscribeToFutureEvents);
                if (!rawSubscription) subscribeError = GetLastError();
            }
            unique_evt_handle subscription{ rawSubscription };
            if (!subscription)
            {
                SetLiveError(subscribeError);
                reportStartup({ false, subscribeError });
                return;
            }

            {
                std::scoped_lock lock{ m_liveMutex };
                m_liveState = LiveState::Running;
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
                    SetLiveError(waitResult == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE);
                    return;
                }
                // Reset before the drain so an arrival racing with EvtNext can
                // signal the event again instead of being cleared afterward.
                if (!ResetEvent(subscriptionEvent.get()))
                {
                    SetLiveError(GetLastError());
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
                            break;
                        }
                        SetLiveError(error);
                        return;
                    }
                    for (DWORD index{}; index < returned; ++index)
                    {
                        ProcessLiveEvent(events[index].get());
                    }
                }
            }
        }
        catch (std::bad_alloc const&)
        {
            SetLiveError(ERROR_OUTOFMEMORY);
            reportStartup({ false, ERROR_OUTOFMEMORY });
        }
        catch (...)
        {
            SetLiveError(ERROR_GEN_FAILURE);
            reportStartup({ false, ERROR_GEN_FAILURE });
        }
    }

    void WindowsRemoteEventService::ProcessLiveEvent(EVT_HANDLE const event)
    {
        auto xml = RenderEvent(event);
        if (xml.empty()) return;

        std::scoped_lock lock{ m_liveMutex };
        if (m_liveState != LiveState::Running && m_liveState != LiveState::EventsLost)
        {
            return;
        }
        ++m_liveTotalReceived;
        if (m_liveEvents.size() >= m_liveQueueLimit)
        {
            m_liveEvents.pop_front();
            ++m_liveDroppedSinceLastBatch;
            m_liveState = LiveState::EventsLost;
        }
        m_liveEvents.emplace_back(std::move(xml));
    }

    void WindowsRemoteEventService::SetLiveError(std::uint32_t const errorCode) noexcept
    {
        try
        {
            std::scoped_lock lock{ m_liveMutex };
            if (m_liveState == LiveState::Stopping)
            {
                return;
            }
            m_liveErrorCode = errorCode == 0 ? ERROR_GEN_FAILURE : errorCode;
            m_liveState = LiveState::Error;
        }
        catch (...)
        {
        }
    }

    std::wstring WindowsRemoteEventService::RenderEvent(EVT_HANDLE const event) const
    {
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &bufferBytes, &propertyCount) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }
        std::vector<wchar_t> buffer(bufferBytes / sizeof(wchar_t) + 1);
        if (!EvtRender(nullptr, event, EvtRenderEventXml, bufferBytes, buffer.data(), &bufferBytes, &propertyCount))
        {
            return {};
        }
        return buffer.data();
    }

    EventQueryResult WindowsRemoteEventService::QueryPage(
        std::wstring_view const channel,
        std::wstring_view const query,
        std::uint32_t const maximumRecords,
        bool const reverseDirection,
        QueryCancellation const& cancellation) const
    {
        EventQueryResult result;
        auto const session = SnapshotSession();
        if (!session)
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
        EVT_HANDLE rawQuery{};
        DWORD queryError{};
        {
            std::scoped_lock apiLock{ session->ApiMutex };
            rawQuery = EvtQuery(session->Handle.get(), channelText.c_str(), queryText.c_str(), flags);
            if (!rawQuery) queryError = GetLastError();
        }
        unique_evt_handle queryHandle{ rawQuery };
        if (!queryHandle)
        {
            result.ErrorCode = queryError;
            result.Status = MapError(result.ErrorCode);
            return result;
        }

        unique_evt_handle renderContext{ EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem) };
        if (!renderContext)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapError(result.ErrorCode);
            return result;
        }

        std::array<EVT_HANDLE, EventBatchSize> rawEvents{};
        while (result.Events.size() < maximumRecords)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            rawEvents.fill(nullptr);
            DWORD returned{};
            auto const succeeded = EvtNext(
                queryHandle.get(),
                static_cast<DWORD>(rawEvents.size()),
                rawEvents.data(),
                QueryPollMilliseconds,
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
                if (error == ERROR_TIMEOUT)
                {
                    continue;
                }
                if (error == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                result.ErrorCode = error;
                result.Status = MapError(error);
                return result;
            }

            for (DWORD index{}; index < returned; ++index)
            {
                if (result.Events.size() < maximumRecords)
                {
                    DWORD renderError{};
                    auto summary = RenderRemoteSummary(
                        events[index].get(),
                        renderContext.get(),
                        renderError);
                    if (!summary)
                    {
                        result.Status = EventQueryStatus::Failed;
                        result.ErrorCode = renderError == ERROR_SUCCESS
                            ? ERROR_EVT_INVALID_EVENT_DATA
                            : renderError;
                        return result;
                    }
                    result.Events.emplace_back(std::move(*summary));
                }
            }
        }
        result.Status = result.Events.empty() ? EventQueryStatus::NoEvents : EventQueryStatus::Succeeded;
        return result;
    }
}
