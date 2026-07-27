// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/LiveEventRecord.h"

#include <cstdint>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace AstralChronicle::services
{
    enum class LiveState
    {
        NotConfigured,
        Ready,
        Starting,
        Stopped,
        Running,
        Paused,
        Stopping,
        Error,
        EventsLost
    };

    struct LiveBatch final
    {
        LiveState State{ LiveState::Stopped };
        std::uint32_t ErrorCode{};
        std::uint32_t DroppedCount{};
        std::uint32_t QueueDepth{};
        std::uint64_t TotalReceived{};
        std::uint32_t CriticalCount{};
        std::uint32_t ErrorCount{};
        std::uint32_t WarningCount{};
        std::chrono::milliseconds Duration{};
        std::vector<std::wstring> Events;
    };

    // Local subscription batches contain a native summary prepared by the data
    // service. Keep this separate from LiveBatch because remote monitoring still
    // transports raw text events over its own service boundary.
    struct EventLiveBatch final
    {
        LiveState State{ LiveState::Stopped };
        std::uint32_t ErrorCode{};
        std::uint32_t DroppedCount{};
        std::uint32_t QueueDepth{};
        std::uint64_t TotalReceived{};
        std::uint32_t CriticalCount{};
        std::uint32_t ErrorCount{};
        std::uint32_t WarningCount{};
        std::chrono::milliseconds Duration{};
        std::vector<models::LiveEventRecord> Events;
    };

    struct EventLiveStatus final
    {
        LiveState State{ LiveState::Stopped };
        std::uint32_t ErrorCode{};
        std::uint32_t DroppedCount{};
        std::uint32_t QueueDepth{};
        std::uint64_t TotalReceived{};
        std::uint32_t CriticalCount{};
        std::uint32_t ErrorCount{};
        std::uint32_t WarningCount{};
        std::chrono::milliseconds Duration{};
    };

    struct IEventLiveService
    {
        virtual ~IEventLiveService() = default;

        virtual bool Start(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) = 0;
        virtual void SetBatchAvailableCallback(std::function<void()> callback) = 0;
        virtual void Pause() noexcept = 0;
        virtual void Resume() noexcept = 0;
        virtual void Stop() noexcept = 0;
        virtual void Clear() noexcept = 0;
        [[nodiscard]] virtual EventLiveStatus Status() const noexcept = 0;
        [[nodiscard]] virtual EventLiveBatch TakeBatch(std::uint32_t maximumEvents) = 0;
    };
}
