// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IRemoteEventService.h"
#include "UniqueEvtHandle.h"

#include <deque>
#include <mutex>
#include <string>

namespace AstralChronicle::services
{
    class WindowsRemoteEventService final : public IRemoteEventService
    {
    public:
        [[nodiscard]] bool Connect(std::wstring_view host, std::wstring_view domain, std::wstring_view user, std::wstring_view password) override;
        void Disconnect() noexcept override;
        [[nodiscard]] bool IsConnected() const noexcept override;
        [[nodiscard]] std::uint32_t LastError() const noexcept override;
        [[nodiscard]] std::wstring_view Host() const noexcept override;
        [[nodiscard]] std::vector<models::EventChannelDescriptor> EnumerateChannels() const override;
        [[nodiscard]] EventQueryResult QueryPage(
            std::wstring_view channel,
            std::wstring_view query,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const override;
        bool StartLive(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) override;
        void StopLive() noexcept override;
        [[nodiscard]] LiveBatch TakeLiveBatch(std::uint32_t maximumEvents) override;

    private:
        static DWORD WINAPI NotificationCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID userContext, EVT_HANDLE event);
        void HandleNotification(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event) noexcept;
        [[nodiscard]] std::wstring RenderEvent(EVT_HANDLE event) const;
        unique_evt_handle m_session;
        unique_evt_handle m_liveSubscription;
        mutable std::mutex m_liveMutex;
        std::deque<std::wstring> m_liveEvents;
        std::uint32_t m_liveQueueLimit{ 5000 };
        std::uint32_t m_liveDroppedCount{};
        std::uint32_t m_liveErrorCode{};
        std::uint64_t m_liveTotalReceived{};
        std::chrono::steady_clock::time_point m_liveStartedAt{};
        LiveState m_liveState{ LiveState::Ready };
        std::wstring m_host;
        std::uint32_t m_lastError{};
    };
}
