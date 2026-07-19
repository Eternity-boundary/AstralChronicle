// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventQueryService.h"

namespace AstralChronicle::services
{
    class WindowsEventQueryService final : public IEventQueryService
    {
    public:
        [[nodiscard]] std::vector<models::EventRecordSummary> QueryRecent(
            std::wstring_view channel,
            std::uint32_t maximumRecords) const override;
    };
}
