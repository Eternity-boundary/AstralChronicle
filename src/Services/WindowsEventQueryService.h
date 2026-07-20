// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventQueryService.h"

namespace AstralChronicle::services
{
    class WindowsEventQueryService final : public IEventQueryService
    {
    public:
        [[nodiscard]] EventQueryResult QueryPage(
            std::wstring_view channel,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] EventQueryResult QueryPageWithQuery(
            std::wstring_view channel,
            std::wstring_view query,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] EventLevelCountsResult QueryLevelCounts(
            std::wstring_view channel,
            std::wstring_view query,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] EventDetailsResult QueryDetails(
            std::wstring_view channel,
            std::uint64_t recordId,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] std::vector<models::EventRecordSummary> QueryRecent(
            std::wstring_view channel,
            std::uint32_t maximumRecords) const override;
    };
}
