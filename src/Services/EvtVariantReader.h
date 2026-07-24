// Created by OpenAI Codex on Jul 23, 2026
#pragma once

#include <windows.h>
#include <winevt.h>

#include <cstdint>
#include <optional>

namespace AstralChronicle::services::details
{
    [[nodiscard]] inline bool IsScalarVariantType(
        EVT_VARIANT const& value,
        EVT_VARIANT_TYPE const expectedType) noexcept
    {
        auto const type = static_cast<DWORD>(value.Type);
        return (type & EVT_VARIANT_TYPE_ARRAY) == 0 &&
            (type & EVT_VARIANT_TYPE_MASK) == static_cast<DWORD>(expectedType);
    }

    [[nodiscard]] inline EVT_VARIANT const* TryGetSystemVariant(
        EVT_VARIANT const* const values,
        DWORD const propertyCount,
        EVT_SYSTEM_PROPERTY_ID const propertyId,
        EVT_VARIANT_TYPE const expectedType) noexcept
    {
        auto const index = static_cast<DWORD>(propertyId);
        if (!values || index >= propertyCount)
        {
            return nullptr;
        }
        auto const& value = values[index];
        return IsScalarVariantType(value, expectedType) ? &value : nullptr;
    }

    [[nodiscard]] inline std::optional<std::uint16_t> ReadEventId(
        EVT_VARIANT const* const values,
        DWORD const propertyCount) noexcept
    {
        auto const eventId = TryGetSystemVariant(
            values,
            propertyCount,
            EvtSystemEventID,
            EvtVarTypeUInt16);
        if (!eventId)
        {
            return std::nullopt;
        }

        return eventId->UInt16Val;
    }

    [[nodiscard]] inline std::optional<std::uint64_t> ReadKeywords(
        EVT_VARIANT const* const values,
        DWORD const propertyCount) noexcept
    {
        if (auto const keywords = TryGetSystemVariant(
                values,
                propertyCount,
                EvtSystemKeywords,
                EvtVarTypeInt64))
        {
            return static_cast<std::uint64_t>(keywords->Int64Val);
        }
        if (auto const keywords = TryGetSystemVariant(
                values,
                propertyCount,
                EvtSystemKeywords,
                EvtVarTypeHexInt64))
        {
            return keywords->UInt64Val;
        }
        if (auto const keywords = TryGetSystemVariant(
                values,
                propertyCount,
                EvtSystemKeywords,
                EvtVarTypeUInt64))
        {
            return keywords->UInt64Val;
        }
        return std::nullopt;
    }
}
