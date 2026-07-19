// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>

namespace AstralChronicle::services
{
    struct IEventLogCatalogService
    {
        virtual ~IEventLogCatalogService() = default;

        [[nodiscard]] virtual std::uint32_t GetAvailableChannelCount() const noexcept = 0;
    };
}
