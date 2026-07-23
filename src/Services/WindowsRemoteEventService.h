// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IRemoteEventService.h"
#include "UniqueEvtHandle.h"

#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <wil/resource.h>

namespace AstralChronicle::services
{
    class WindowsRemoteEventService final : public IRemoteEventService
    {
    public:
        ~WindowsRemoteEventService() override;

        [[nodiscard]] bool Connect(std::wstring_view host, std::wstring_view domain, std::wstring_view user, std::wstring_view password) override;
        [[nodiscard]] RemoteProbeResult Probe(
            std::wstring_view host,
            std::wstring_view domain,
            std::wstring_view user,
            std::wstring_view password) const override;
        void Disconnect() noexcept override;
        [[nodiscard]] bool IsConnected() const noexcept override;
        [[nodiscard]] std::uint32_t LastError() const noexcept override;
        [[nodiscard]] std::wstring Host() const override;
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
        struct SessionState final
        {
            SessionState(unique_evt_handle session, std::wstring host, std::uint64_t generation)
                : Handle(std::move(session)), Host(std::move(host)), Generation(generation)
            {
            }

            unique_evt_handle Handle;
            mutable std::mutex ApiMutex;
            std::wstring Host;
            std::uint64_t Generation{};
        };

        struct SessionOpenResult final
        {
            unique_evt_handle Session;
            std::uint32_t ErrorCode{};
        };

        struct WorkerStartResult final
        {
            bool Success{};
            std::uint32_t ErrorCode{};
        };

        [[nodiscard]] static SessionOpenResult OpenValidatedSession(
            std::wstring_view host,
            std::wstring_view domain,
            std::wstring_view user,
            std::wstring_view password);
        [[nodiscard]] static std::uint32_t ValidateSession(EVT_HANDLE session);
        [[nodiscard]] std::shared_ptr<SessionState> SnapshotSession() const;
        void RunLiveWorker(
            std::stop_token stopToken,
            HANDLE stopEvent,
            std::shared_ptr<SessionState> session,
            std::wstring channel,
            std::wstring query,
            std::promise<WorkerStartResult> startup);
        void ProcessLiveEvent(EVT_HANDLE event);
        void SetLiveError(std::uint32_t errorCode) noexcept;
        void StopLiveWorkerLocked() noexcept;
        [[nodiscard]] std::wstring RenderEvent(EVT_HANDLE event) const;

        mutable std::mutex m_stateMutex;
        std::shared_ptr<SessionState> m_session;
        std::uint64_t m_generation{};
        std::uint32_t m_lastError{};

        mutable std::mutex m_liveMutex;
        std::deque<std::wstring> m_liveEvents;
        std::uint32_t m_liveQueueLimit{ 5000 };
        std::uint32_t m_liveDroppedSinceLastBatch{};
        std::uint32_t m_liveErrorCode{};
        std::uint64_t m_liveTotalReceived{};
        std::chrono::steady_clock::time_point m_liveStartedAt{};
        LiveState m_liveState{ LiveState::Ready };

        std::mutex m_liveWorkerMutex;
        wil::unique_handle m_liveStopEvent;
        std::jthread m_liveWorker;
    };
}
