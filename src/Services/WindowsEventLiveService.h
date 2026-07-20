// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventLiveService.h"
#include "UniqueEvtHandle.h"

#include <mutex>
#include <deque>
#include <chrono>

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
        static DWORD WINAPI NotificationCallback(
            EVT_SUBSCRIBE_NOTIFY_ACTION action,
            PVOID userContext,
            EVT_HANDLE event);
        void HandleNotification(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event) noexcept;
        [[nodiscard]] std::wstring RenderEvent(EVT_HANDLE event) const;

        unique_evt_handle m_subscription;
        mutable std::mutex m_mutex;
        std::deque<std::wstring> m_events;
        std::wstring m_channel;
        std::wstring m_query;
        std::uint32_t m_queueLimit{ 5000 };
        std::uint32_t m_droppedCount{};
        std::uint32_t m_errorCode{};
        std::uint64_t m_totalReceived{};
        std::uint32_t m_criticalCount{};
        std::uint32_t m_errorCount{};
        std::uint32_t m_warningCount{};
        std::chrono::steady_clock::time_point m_startedAt{};
        LiveState m_state{ LiveState::Ready };
    };
}
