// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include <string>
#include <unordered_set>

namespace AstralChronicle::services
{
    struct IEventBookmarkStore
    {
        virtual ~IEventBookmarkStore() = default;

        [[nodiscard]] virtual std::unordered_set<std::wstring> Load() const = 0;
        virtual void Save(std::unordered_set<std::wstring> const& bookmarks) const noexcept = 0;
        virtual void Clear() const noexcept = 0;
    };
}
