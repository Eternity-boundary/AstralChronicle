// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventChannelDescriptor.h"

#include <optional>
#include <string>
#include <vector>

namespace AstralChronicle::services
{
    struct ChannelPathTreeNode final
    {
        std::wstring Segment;
        std::wstring FullPath;
        std::optional<models::EventChannelDescriptor> Channel;
        std::vector<ChannelPathTreeNode> Children;
    };

    [[nodiscard]] std::vector<ChannelPathTreeNode> BuildChannelPathTree(
        std::vector<models::EventChannelDescriptor> const& channels);
}
