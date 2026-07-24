// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventLiveService.h"
#include "UniqueEvtHandle.h"

#include <mutex>
#include <deque>
#include <chrono>
#include <future>
#include <thread>

#include <wil/resource.h>

namespace AstralChronicle::services
{
    class WindowsEventLiveService final : public IEventLiveService
    {
    public:
        ~WindowsEventLiveService() override;

        bool Start(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) override;
        void Pause() noexcept override;
        void Resume() noexcept override;
        void Stop() noexcept override;
        void Clear() noexcept override;
        [[nodiscard]] LiveBatch TakeBatch(std::uint32_t maximumEvents) override;

    private:
        struct WorkerStartResult final
        {
            bool Success{};
            std::uint32_t ErrorCode{};
        };

        void RunWorker(
            std::stop_token stopToken,
            HANDLE stopEvent,
            std::wstring channel,
            std::wstring query,
            std::promise<WorkerStartResult> startup);
        void ProcessEvent(EVT_HANDLE event);
        void SetWorkerError(std::uint32_t errorCode) noexcept;
        void StopWorkerLocked() noexcept;
        [[nodiscard]] std::wstring RenderEvent(EVT_HANDLE event) const;

        mutable std::mutex m_mutex;
        std::deque<std::wstring> m_events;
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

        std::mutex m_workerMutex;
        wil::unique_handle m_stopEvent;
        std::jthread m_worker;
    };
}
