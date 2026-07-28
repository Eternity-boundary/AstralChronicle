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

        [[nodiscard]] EventQueryResult QueryPageWithQueryOffset(
            std::wstring_view channel,
            std::wstring_view query,
            std::uint32_t skippedRecords,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] EventQueryResult QuerySavedLogPageWithQueryOffset(
            std::wstring_view filePath,
            std::wstring_view query,
            std::uint32_t skippedRecords,
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

        [[nodiscard]] EventDetailsResult QuerySavedLogDetails(
            std::wstring_view filePath,
            std::uint64_t recordId,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] std::vector<models::EventRecordSummary> QueryRecent(
            std::wstring_view channel,
            std::uint32_t maximumRecords) const override;

    private:
        [[nodiscard]] EventQueryResult QueryPageWithQueryOffsetCore(
            std::wstring_view path,
            std::wstring_view query,
            std::uint32_t skippedRecords,
            std::uint32_t maximumRecords,
            bool reverseDirection,
            bool savedLogFile,
            QueryCancellation const& cancellation) const;

        [[nodiscard]] EventDetailsResult QueryDetailsCore(
            std::wstring_view path,
            std::uint64_t recordId,
            bool savedLogFile,
            QueryCancellation const& cancellation) const;
    };
}
