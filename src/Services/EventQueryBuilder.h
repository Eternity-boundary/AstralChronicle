// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventFilter.h"

#include <optional>
#include <string>
#include <string_view>

namespace AstralChronicle::services
{
    [[nodiscard]] std::wstring BuildEventQuery(
        models::EventFilter const& filter);

    [[nodiscard]] std::optional<std::wstring> CombineEventQueries(
        std::wstring_view baseQuery,
        std::wstring_view additionalQuery);

    [[nodiscard]] std::optional<std::wstring> ApplyFilterToStructuredQuery(
        std::wstring_view queryList,
        std::wstring_view additionalQuery);
}
