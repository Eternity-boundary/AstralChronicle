// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <chrono>
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

    struct IEventLiveService
    {
        virtual ~IEventLiveService() = default;

        virtual bool Start(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) = 0;
        virtual void Pause() noexcept = 0;
        virtual void Resume() noexcept = 0;
        virtual void Stop() noexcept = 0;
        virtual void Clear() noexcept = 0;
        [[nodiscard]] virtual LiveBatch TakeBatch(std::uint32_t maximumEvents) = 0;
    };
}
