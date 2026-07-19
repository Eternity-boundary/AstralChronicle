// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventRecordSummary.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace AstralChronicle::services
{
    struct IEventQueryService
    {
        virtual ~IEventQueryService() = default;

        [[nodiscard]] virtual std::vector<models::EventRecordSummary> QueryRecent(
            std::wstring_view channel,
            std::uint32_t maximumRecords) const = 0;
    };
}
