// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <string>

namespace AstralChronicle::models
{
    struct EventRecordSummary final
    {
        std::uint64_t RecordId{};
        std::uint16_t EventId{};
        std::uint8_t Level{};
        std::wstring Provider;
        std::wstring Channel;
    };
}
