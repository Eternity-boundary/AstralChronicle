// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventChannelDescriptor.h"
#include "Models/EventFilter.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    // Creates a QueryList that reads every enabled channel that the current user can open.
    // The caller can then apply its own cancellable, paged result filtering without first
    // materializing the event tree in the navigation pane.
    [[nodiscard]] std::optional<std::wstring> BuildAvailableChannelsQueryList(
        std::vector<models::EventChannelDescriptor> const& channels);
}
