// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include <cstdint>
#include <string_view>

namespace AstralChronicle::services
{
    // Encapsulates application-local preferences and the storage lifecycle rules
    // associated with disabling persisted feature data.
    struct IApplicationPreferencesService
    {
        virtual ~IApplicationPreferencesService() = default;

        [[nodiscard]] virtual bool ReadBool(
            std::wstring_view key,
            bool fallback) const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t ReadUInt32(
            std::wstring_view key,
            std::uint32_t fallback,
            std::uint32_t minimum,
            std::uint32_t maximum) const noexcept = 0;
        virtual void WriteBool(std::wstring_view key, bool value) const noexcept = 0;
        virtual void WriteText(std::wstring_view key, std::wstring_view value) const noexcept = 0;
        virtual void ClearPersistedSessions() const noexcept = 0;
        virtual void ClearPersistedBookmarks() const noexcept = 0;
    };
}
