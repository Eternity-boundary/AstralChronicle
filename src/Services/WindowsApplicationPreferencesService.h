// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include "IApplicationPreferencesService.h"

namespace AstralChronicle::services
{
    class WindowsApplicationPreferencesService final : public IApplicationPreferencesService
    {
    public:
        [[nodiscard]] bool ReadBool(
            std::wstring_view key,
            bool fallback) const noexcept override;
        [[nodiscard]] std::uint32_t ReadUInt32(
            std::wstring_view key,
            std::uint32_t fallback,
            std::uint32_t minimum,
            std::uint32_t maximum) const noexcept override;
        void WriteBool(std::wstring_view key, bool value) const noexcept override;
        void WriteText(std::wstring_view key, std::wstring_view value) const noexcept override;
        void ClearPersistedSessions() const noexcept override;
        void ClearPersistedBookmarks() const noexcept override;
    };
}
