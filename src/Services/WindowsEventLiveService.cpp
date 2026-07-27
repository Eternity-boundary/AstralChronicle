// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventLiveService.h"

#include <winevt.h>

#include <algorithm>
#include <cstddef>
#include <new>
#include <string_view>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace AstralChronicle::services
{
    namespace
    {
        [[nodiscard]] bool IsStructuredQuery(std::wstring_view query)
        {
            auto const start = query.find_first_not_of(L" \t\r\n");
            return start != std::wstring_view::npos &&
                query.compare(start, 10, L"<QueryList") == 0;
        }

    }

    WindowsEventLiveService::WindowsEventLiveService(
        std::shared_ptr<IEventLiveDataService> eventData)
        : m_eventData(std::move(eventData))
    {
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
        if (!m_eventData)
        {
            SetWorkerError(ERROR_INVALID_HANDLE);
            return false;
        }
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
            auto context = std::make_shared<SubscriptionCallbackContext>();
            context->Service = this;
            context->StopEvent = stopEvent;
            m_worker = std::jthread(
                [this,
                 stopEvent,
                 channel = std::move(channelText),
                 query = std::move(queryText),
                 context = std::move(context),
                 startup = std::move(startup)](std::stop_token const stopToken) mutable
                {
                    RunWorker(
                        stopToken,
                        stopEvent,
                        std::move(channel),
                        std::move(query),
                        std::move(context),
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

    void WindowsEventLiveService::SetBatchAvailableCallback(std::function<void()> callback)
    {
        std::scoped_lock lock{ m_mutex };
        m_batchAvailableCallback = std::move(callback);
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

    EventLiveStatus WindowsEventLiveService::Status() const noexcept
    {
        std::scoped_lock lock{ m_mutex };
        EventLiveStatus status;
        status.State = m_state;
        status.ErrorCode = m_errorCode;
        status.DroppedCount = m_droppedSinceLastBatch;
        status.QueueDepth = static_cast<std::uint32_t>(m_events.size());
        status.TotalReceived = m_totalReceived;
        status.CriticalCount = m_criticalCount;
        status.ErrorCount = m_errorCount;
        status.WarningCount = m_warningCount;
        status.Duration = m_startedAt.time_since_epoch().count() == 0
            ? std::chrono::milliseconds{}
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startedAt);
        return status;
    }

    EventLiveBatch WindowsEventLiveService::TakeBatch(std::uint32_t const maximumEvents)
    {
        EventLiveBatch result;
        bool notify{};
        {
            std::scoped_lock lock{ m_mutex };
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
            notify = maximumEvents != 0 && !m_events.empty();
        }
        if (notify)
        {
            NotifyBatchAvailable();
        }
        return result;
    }

    void WindowsEventLiveService::RunWorker(
        std::stop_token const stopToken,
        HANDLE const stopEvent,
        std::wstring channel,
        std::wstring query,
        std::shared_ptr<SubscriptionCallbackContext> context,
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

        auto failStartup = [this, &reportStartup](DWORD const errorCode) noexcept
        {
            auto const actualError = errorCode == ERROR_SUCCESS
                ? ERROR_GEN_FAILURE
                : errorCode;
            SetWorkerError(actualError);
            reportStartup({ false, actualError });
        };

        unique_evt_handle subscription;
        try
        {
            // EvtSubscribe may deliver immediately, so make the queue eligible
            // before giving the service a callback context.
            {
                std::scoped_lock lock{ m_mutex };
                m_state = m_pauseRequested ? LiveState::Paused : LiveState::Running;
            }

            auto const structuredQuery = IsStructuredQuery(query);
            subscription.reset(EvtSubscribe(
                nullptr,
                nullptr,
                structuredQuery ? nullptr : channel.c_str(),
                query.c_str(),
                nullptr,
                context.get(),
                SubscriptionCallback,
                EvtSubscribeToFutureEvents | EvtSubscribeStrict));
            if (!subscription)
            {
                auto const errorCode = GetLastError();
                StopSubscription(subscription, context);
                failStartup(errorCode);
                return;
            }

            DWORD callbackError{};
            {
                std::scoped_lock lock{ m_mutex };
                if (m_state == LiveState::Error)
                {
                    callbackError = m_errorCode == ERROR_SUCCESS
                        ? ERROR_GEN_FAILURE
                        : m_errorCode;
                }
            }
            if (callbackError != ERROR_SUCCESS)
            {
                StopSubscription(subscription, context);
                reportStartup({ false, callbackError });
                return;
            }

            reportStartup({ true, ERROR_SUCCESS });
            auto const waitResult = WaitForSingleObject(stopEvent, INFINITE);
            if (waitResult != WAIT_OBJECT_0 && !stopToken.stop_requested())
            {
                SetWorkerError(waitResult == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE);
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
        StopSubscription(subscription, context);
    }

    DWORD WINAPI WindowsEventLiveService::SubscriptionCallback(
        EVT_SUBSCRIBE_NOTIFY_ACTION const action,
        PVOID const userContext,
        EVT_HANDLE const event) noexcept
    {
        auto const context = static_cast<SubscriptionCallbackContext*>(userContext);
        if (!context) return ERROR_INVALID_PARAMETER;

        WindowsEventLiveService* service{};
        HANDLE stopEvent{};
        {
            std::scoped_lock lock{ context->Mutex };
            if (context->Stopping || !context->Service)
            {
                return ERROR_SUCCESS;
            }
            ++context->ActiveCallbacks;
            service = context->Service;
            stopEvent = context->StopEvent;
        }

        auto releaseCallback = [context]() noexcept
        {
            std::scoped_lock lock{ context->Mutex };
            --context->ActiveCallbacks;
            if (context->ActiveCallbacks == 0)
            {
                context->CallbacksDrained.notify_all();
            }
        };

        try
        {
            switch (action)
            {
            case EvtSubscribeActionDeliver:
            {
                // The Event Log service owns this handle and closes it when this
                // callback returns. Render the XML before handing work to the queue.
                DWORD errorCode{};
                auto rendered = service->RenderEvent(event, errorCode);
                if (rendered.empty())
                {
                    service->SetWorkerError(errorCode == ERROR_SUCCESS
                        ? ERROR_EVT_INVALID_EVENT_DATA
                        : errorCode);
                    if (stopEvent) SetEvent(stopEvent);
                    break;
                }
                auto record = service->m_eventData->CreateRecord(std::move(rendered));
                (void)service->EnqueueRenderedEvent(std::move(record));
                break;
            }

            case EvtSubscribeActionError:
                service->SetWorkerError(
                    static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(event)));
                if (stopEvent) SetEvent(stopEvent);
                break;

            default:
                service->SetWorkerError(ERROR_INVALID_PARAMETER);
                if (stopEvent) SetEvent(stopEvent);
                break;
            }
        }
        catch (std::bad_alloc const&)
        {
            service->SetWorkerError(ERROR_OUTOFMEMORY);
            if (stopEvent) SetEvent(stopEvent);
        }
        catch (...)
        {
            service->SetWorkerError(ERROR_GEN_FAILURE);
            if (stopEvent) SetEvent(stopEvent);
        }

        releaseCallback();
        return ERROR_SUCCESS;
    }

    void WindowsEventLiveService::StopSubscription(
        unique_evt_handle& subscription,
        std::shared_ptr<SubscriptionCallbackContext> const& context) noexcept
    {
        if (!context)
        {
            subscription.reset();
            return;
        }

        {
            std::scoped_lock lock{ context->Mutex };
            context->Stopping = true;
        }
        // Closing the subscription cancels delivery. Do not hold the context
        // mutex here because EvtClose may wait for an in-flight callback.
        subscription.reset();

        std::unique_lock lock{ context->Mutex };
        context->CallbacksDrained.wait(lock, [context]
        {
            return context->ActiveCallbacks == 0;
        });
        context->Service = nullptr;
        context->StopEvent = nullptr;
    }

    bool WindowsEventLiveService::EnqueueRenderedEvent(models::LiveEventRecord event)
    {
        bool queued{};
        bool notify{};
        {
            std::scoped_lock lock{ m_mutex };
            if (m_state != LiveState::Running &&
                m_state != LiveState::Paused &&
                m_state != LiveState::EventsLost)
            {
                return false;
            }

            ++m_totalReceived;
            if (event.Summary.Level == 1) ++m_criticalCount;
            else if (event.Summary.Level == 2) ++m_errorCount;
            else if (event.Summary.Level == 3) ++m_warningCount;
            if (m_events.size() >= m_queueLimit)
            {
                m_events.pop_front();
                ++m_droppedSinceLastBatch;
                m_state = LiveState::EventsLost;
            }
            notify = m_events.empty();
            m_events.emplace_back(std::move(event));
            queued = true;
        }
        if (notify)
        {
            NotifyBatchAvailable();
        }
        return queued;
    }

    void WindowsEventLiveService::SetWorkerError(std::uint32_t const errorCode) noexcept
    {
        bool notify{};
        try
        {
            std::scoped_lock lock{ m_mutex };
            if (m_state == LiveState::Stopping)
            {
                return;
            }
            m_errorCode = errorCode == 0 ? ERROR_GEN_FAILURE : errorCode;
            m_state = LiveState::Error;
            notify = true;
        }
        catch (...)
        {
        }
        if (notify)
        {
            NotifyBatchAvailable();
        }
    }

    void WindowsEventLiveService::NotifyBatchAvailable() noexcept
    {
        try
        {
            std::function<void()> callback;
            {
                std::scoped_lock lock{ m_mutex };
                callback = m_batchAvailableCallback;
            }
            if (callback)
            {
                callback();
            }
        }
        catch (...)
        {
        }
    }

    std::wstring WindowsEventLiveService::RenderEvent(
        EVT_HANDLE const event,
        DWORD& errorCode) const
    {
        errorCode = ERROR_SUCCESS;
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &bufferBytes, &propertyCount))
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return {};
        }
        errorCode = GetLastError();
        if (errorCode != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }
        std::vector<wchar_t> buffer(
            (static_cast<std::size_t>(bufferBytes) + sizeof(wchar_t) - 1) /
                sizeof(wchar_t) +
            1);
        if (!EvtRender(nullptr, event, EvtRenderEventXml, bufferBytes, buffer.data(), &bufferBytes, &propertyCount))
        {
            errorCode = GetLastError();
            return {};
        }
        errorCode = ERROR_SUCCESS;
        return buffer.front() == L'\0' ? std::wstring{} : std::wstring{ buffer.data() };
    }
}
