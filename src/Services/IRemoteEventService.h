// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include "Models/EventChannelDescriptor.h"
#include "IEventQueryService.h"
#include "IEventLiveService.h"
#include <string>
#include <vector>
#include <string_view>

namespace AstralChronicle::services
{
    struct RemoteProbeResult final
    {
        bool Success{};
        std::uint32_t ErrorCode{};
    };

    struct IRemoteEventService
    {
        virtual ~IRemoteEventService() = default;
        [[nodiscard]] virtual bool Connect(
            std::wstring_view host,
            std::wstring_view domain,
            std::wstring_view user,
            std::wstring_view password) = 0;
        [[nodiscard]] virtual RemoteProbeResult Probe(
            std::wstring_view host,
            std::wstring_view domain,
            std::wstring_view user,
            std::wstring_view password) const = 0;
        virtual void Disconnect() noexcept = 0;
        [[nodiscard]] virtual bool IsConnected() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t LastError() const noexcept = 0;
        [[nodiscard]] virtual std::wstring Host() const = 0;
        [[nodiscard]] virtual std::vector<models::EventChannelDescriptor> EnumerateChannels() const = 0;
        [[nodiscard]] virtual EventQueryResult QueryPage(
            std::wstring_view channel,
            std::wstring_view query,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const = 0;
        virtual bool StartLive(std::wstring_view channel, std::wstring_view query, std::uint32_t queueLimit) = 0;
        virtual void StopLive() noexcept = 0;
        [[nodiscard]] virtual LiveBatch TakeLiveBatch(std::uint32_t maximumEvents) = 0;
    };
}
