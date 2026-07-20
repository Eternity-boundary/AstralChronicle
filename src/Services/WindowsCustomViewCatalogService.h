// Created by EternityBoundary on Jul 21,2026
#pragma once

#include "ICustomViewCatalogService.h"

namespace AstralChronicle::services
{
    class WindowsCustomViewCatalogService final : public ICustomViewCatalogService
    {
    public:
        [[nodiscard]] std::vector<models::CustomViewTreeNode> Enumerate() const override;
    };
}
