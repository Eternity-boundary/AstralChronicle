// Created by OpenAI Codex on Jul 23, 2026
#include "../src/Services/EvtVariantReader.h"
#include "../src/Models/EventRecordSummary.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <type_traits>

int main()
{
    using AstralChronicle::services::details::IsScalarVariantType;
    using AstralChronicle::services::details::ReadEventId;
    using AstralChronicle::services::details::ReadKeywords;
    using AstralChronicle::services::details::TryGetSystemVariant;

    static_assert(std::is_same_v<
        decltype(AstralChronicle::models::EventRecordSummary::EventId),
        std::uint16_t>);

    EVT_VARIANT value{};
    value.Type = EvtVarTypeUInt16;
    value.UInt16Val = 41;
    assert(IsScalarVariantType(value, EvtVarTypeUInt16));
    assert(!IsScalarVariantType(value, EvtVarTypeUInt32));

    value.Type = EvtVarTypeNull;
    value.UInt16Val = 41;
    assert(!IsScalarVariantType(value, EvtVarTypeUInt16));

    value.Type = EvtVarTypeUInt16 | EVT_VARIANT_TYPE_ARRAY;
    assert(!IsScalarVariantType(value, EvtVarTypeUInt16));

    constexpr auto propertyCount = static_cast<std::size_t>(EvtSystemPropertyIdEND);
    std::array<EVT_VARIANT, propertyCount> properties{};
    properties[EvtSystemEventID].Type = EvtVarTypeUInt16;
    properties[EvtSystemEventID].UInt16Val = 0x1234;
    properties[EvtSystemQualifiers].Type = EvtVarTypeUInt16;
    properties[EvtSystemQualifiers].UInt16Val = 0xABCD;

    auto const qualified = ReadEventId(
        properties.data(),
        static_cast<DWORD>(properties.size()));
    assert(qualified && *qualified == 0x1234u);

    properties[EvtSystemQualifiers].Type = EvtVarTypeNull;
    auto const unqualified = ReadEventId(
        properties.data(),
        static_cast<DWORD>(properties.size()));
    assert(unqualified && *unqualified == 0x1234u);

    properties[EvtSystemEventID].Type = EvtVarTypeUInt32;
    assert(!ReadEventId(
        properties.data(),
        static_cast<DWORD>(properties.size())));

    properties[EvtSystemKeywords].Type = EvtVarTypeInt64;
    properties[EvtSystemKeywords].Int64Val =
        static_cast<std::int64_t>(0x8123456789ABCDEFULL);
    auto const signedKeywords = ReadKeywords(
        properties.data(),
        static_cast<DWORD>(properties.size()));
    assert(signedKeywords && *signedKeywords == 0x8123456789ABCDEFULL);

    properties[EvtSystemKeywords].Type = EvtVarTypeHexInt64;
    properties[EvtSystemKeywords].UInt64Val = 0x42u;
    auto const hexadecimalKeywords = ReadKeywords(
        properties.data(),
        static_cast<DWORD>(properties.size()));
    assert(hexadecimalKeywords && *hexadecimalKeywords == 0x42u);

    assert(!TryGetSystemVariant(
        properties.data(),
        static_cast<DWORD>(EvtSystemEventID),
        EvtSystemEventID,
        EvtVarTypeUInt32));
    return 0;
}
