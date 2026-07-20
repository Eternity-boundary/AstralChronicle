// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/SavedView.h"

#include <string_view>
#include <vector>

namespace AstralChronicle::services
{
    struct ISavedViewRepository
    {
        virtual ~ISavedViewRepository() = default;

        [[nodiscard]] virtual std::vector<models::SavedView> Load() const = 0;
        virtual bool Upsert(models::SavedView const& view) = 0;
        virtual bool Remove(std::wstring_view id) = 0;
    };
}
