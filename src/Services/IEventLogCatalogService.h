// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventChannelDescriptor.h"

#include <cstdint>
#include <vector>

namespace AstralChronicle::services
{
    struct IEventLogCatalogService
    {
        virtual ~IEventLogCatalogService() = default;

        [[nodiscard]] virtual std::uint32_t GetAvailableChannelCount() const noexcept = 0;
        [[nodiscard]] virtual std::vector<models::EventChannelDescriptor> EnumerateChannels() const = 0;
    };
}
