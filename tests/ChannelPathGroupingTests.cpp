// Created by EternityBoundary on Jul 20,2026
#include "../src/Services/ChannelPathGrouping.h"

#include <cassert>
#include <string>
#include <vector>

using AstralChronicle::models::EventChannelDescriptor;
using AstralChronicle::models::EventChannelState;
using AstralChronicle::services::BuildChannelPathTree;

int main()
{
    std::vector<EventChannelDescriptor> channels{
        { L"Microsoft\\Windows\\Alpha/Operational", EventChannelState::Available },
        { L"Microsoft\\Windows\\Beta/Operational", EventChannelState::Disabled },
        { L"Vendor/Devices/Diagnostics", EventChannelState::AccessDenied },
        { L"RootChannel", EventChannelState::Unavailable } };

    auto const tree = BuildChannelPathTree(channels);
    assert(tree.size() == 3);
    assert(tree[0].Segment == L"Microsoft");
    assert(tree[0].Children.size() == 1);
    assert(tree[0].Children[0].Segment == L"Windows");
    assert(tree[0].Children[0].Children.size() == 2);
    assert(tree[0].Children[0].Children[0].Segment == L"Alpha");
    assert(tree[0].Children[0].Children[0].Children[0].Channel.has_value());
    assert(tree[0].Children[0].Children[0].Children[0].Channel->State == EventChannelState::Available);
    assert(tree[1].Segment == L"RootChannel");
    assert(tree[1].Channel.has_value());
    assert(tree[2].Segment == L"Vendor");
    assert(tree[2].Children[0].Children[0].Channel->State == EventChannelState::AccessDenied);
    return 0;
}
