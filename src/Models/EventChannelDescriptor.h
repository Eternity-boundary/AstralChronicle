// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <string>

namespace AstralChronicle::models
{
    enum class EventChannelState
    {
        Available,
        Disabled,
        AccessDenied,
        Unavailable,
    };

    struct EventChannelIdentifier final
    {
        std::wstring Path;
    };

    struct EventChannelDescriptor final
    {
        std::wstring Path;
        EventChannelState State{ EventChannelState::Unavailable };
    };
}
