// Created by EternityBoundary on Jul 21,2026
#pragma once

#include "Models/CustomViewTreeNode.h"

#include <vector>

namespace AstralChronicle::services
{
    struct ICustomViewCatalogService
    {
        virtual ~ICustomViewCatalogService() = default;

        [[nodiscard]] virtual std::vector<models::CustomViewTreeNode> Enumerate() const = 0;
    };
}
