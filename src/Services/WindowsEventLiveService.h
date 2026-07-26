// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventLiveService.h"
#include "IEventLiveDataService.h"
#include "UniqueEvtHandle.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include <wil/resource.h>

namespace AstralChronicle::services
{
    class WindowsEventLiveService final : public IEventLiveService
    {
    public:
        explicit WindowsEventLiveService(std::shared_ptr<IEventLiveDataService> eventData);
        ~WindowsEventLiveService() override;

        bool Start(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) override;
        void SetBatchAvailableCallback(std::function<void()> callback) override;
        void Pause() noexcept override;
        void Resume() noexcept override;
        void Stop() noexcept override;
        void Clear() noexcept override;
        [[nodiscard]] EventLiveBatch TakeBatch(std::uint32_t maximumEvents) override;

    private:
        struct WorkerStartResult final
        {
            bool Success{};
            std::uint32_t ErrorCode{};
        };

        struct SubscriptionCallbackContext final
        {
            std::mutex Mutex;
            std::condition_variable CallbacksDrained;
            WindowsEventLiveService* Service{};
            HANDLE StopEvent{};
            std::uint32_t ActiveCallbacks{};
            bool Stopping{};
        };

        void RunWorker(
            std::stop_token stopToken,
            HANDLE stopEvent,
            std::wstring channel,
            std::wstring query,
            std::shared_ptr<SubscriptionCallbackContext> context,
            std::promise<WorkerStartResult> startup);
        static DWORD WINAPI SubscriptionCallback(
            EVT_SUBSCRIBE_NOTIFY_ACTION action,
            PVOID userContext,
            EVT_HANDLE event) noexcept;
        static void StopSubscription(
            unique_evt_handle& subscription,
            std::shared_ptr<SubscriptionCallbackContext> const& context) noexcept;
        [[nodiscard]] bool EnqueueRenderedEvent(models::LiveEventRecord event);
        void SetWorkerError(std::uint32_t errorCode) noexcept;
        void StopWorkerLocked() noexcept;
        void NotifyBatchAvailable() noexcept;
        [[nodiscard]] std::wstring RenderEvent(EVT_HANDLE event, DWORD& errorCode) const;

        mutable std::mutex m_mutex;
        std::shared_ptr<IEventLiveDataService> m_eventData;
        std::deque<models::LiveEventRecord> m_events;
        std::uint32_t m_queueLimit{ 5000 };
        std::uint32_t m_droppedSinceLastBatch{};
        std::uint32_t m_errorCode{};
        std::uint64_t m_totalReceived{};
        std::uint32_t m_criticalCount{};
        std::uint32_t m_errorCount{};
        std::uint32_t m_warningCount{};
        std::chrono::steady_clock::time_point m_startedAt{};
        LiveState m_state{ LiveState::Ready };
        bool m_pauseRequested{};
        std::function<void()> m_batchAvailableCallback;

        std::mutex m_workerMutex;
        wil::unique_handle m_stopEvent;
        std::jthread m_worker;
    };
}
