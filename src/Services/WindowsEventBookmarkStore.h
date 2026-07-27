// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include "IEventBookmarkStore.h"

namespace AstralChronicle::services
{
    class WindowsEventBookmarkStore final : public IEventBookmarkStore
    {
    public:
        [[nodiscard]] std::unordered_set<std::wstring> Load() const override;
        void Save(std::unordered_set<std::wstring> const& bookmarks) const noexcept override;
        void Clear() const noexcept override;
    };
}
