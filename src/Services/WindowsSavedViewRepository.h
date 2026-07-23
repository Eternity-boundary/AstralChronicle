// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "ISavedViewRepository.h"

#include <mutex>

namespace AstralChronicle::services
{
    class WindowsSavedViewRepository final : public ISavedViewRepository
    {
    public:
        [[nodiscard]] std::vector<models::SavedView> Load() const override;
        bool Upsert(models::SavedView const& view) override;
        bool Remove(std::wstring_view id) override;

    private:
        mutable std::mutex m_mutex;
    };
}
