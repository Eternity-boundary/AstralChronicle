// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventLogCatalogService.h"

namespace AstralChronicle::services
{
    class WindowsEventLogCatalogService final : public IEventLogCatalogService
    {
    public:
        [[nodiscard]] std::uint32_t GetAvailableChannelCount() const noexcept override;
        [[nodiscard]] std::vector<models::EventChannelDescriptor> EnumerateChannels() const override;
    };
}
