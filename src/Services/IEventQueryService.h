// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventRecordSummary.h"
#include "Models/EventDetails.h"

#include <cstdint>
#include <atomic>
#include <memory>
#include <string_view>
#include <vector>

namespace AstralChronicle::services
{
    enum class EventQueryStatus
    {
        Succeeded,
        NoEvents,
        Cancelled,
        AccessDenied,
        InvalidChannel,
        ServiceUnavailable,
        Failed,
    };

    using QueryCancellation = std::shared_ptr<std::atomic_bool>;

    struct EventQueryResult final
    {
        EventQueryStatus Status{ EventQueryStatus::Failed };
        std::uint32_t ErrorCode{};
        std::vector<models::EventRecordSummary> Events;
    };

    struct EventDetailsResult final
    {
        EventQueryStatus Status{ EventQueryStatus::Failed };
        std::uint32_t ErrorCode{};
        models::EventDetails Details;
    };

    struct EventLevelCounts final
    {
        std::uint32_t Critical{};
        std::uint32_t Error{};
        std::uint32_t Warning{};
        std::uint32_t Total{};
    };

    struct EventLevelCountsResult final
    {
        EventQueryStatus Status{ EventQueryStatus::Failed };
        std::uint32_t ErrorCode{};
        EventLevelCounts Counts;
    };

    [[nodiscard]] inline QueryCancellation MakeQueryCancellation()
    {
        return std::make_shared<std::atomic_bool>(false);
    }

    struct IEventQueryService
    {
        virtual ~IEventQueryService() = default;

        [[nodiscard]] virtual EventQueryResult QueryPage(
            std::wstring_view channel,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const = 0;

        [[nodiscard]] virtual EventQueryResult QueryPageWithQuery(
            std::wstring_view channel,
            std::wstring_view query,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const = 0;

        [[nodiscard]] virtual EventLevelCountsResult QueryLevelCounts(
            std::wstring_view channel,
            std::wstring_view query,
            QueryCancellation const& cancellation) const = 0;

        [[nodiscard]] virtual EventDetailsResult QueryDetails(
            std::wstring_view channel,
            std::uint64_t recordId,
            QueryCancellation const& cancellation) const = 0;

        [[nodiscard]] virtual std::vector<models::EventRecordSummary> QueryRecent(
            std::wstring_view channel,
            std::uint32_t maximumRecords) const
        {
            auto const result = QueryPage(channel, maximumRecords, true, MakeQueryCancellation());
            return result.Events;
        }
    };
}
