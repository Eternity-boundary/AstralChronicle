// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace AstralChronicle::models
{
    struct EventRecordSummary final
    {
        std::uint64_t RecordId{};
        std::uint16_t EventId{};
        std::uint8_t Version{};
        std::uint8_t Level{};
        std::uint8_t Opcode{};
        std::uint64_t Keywords{};
        std::chrono::system_clock::time_point TimeCreated{};
        std::wstring Provider;
        std::wstring ProviderGuid;
        std::wstring Channel;
        std::wstring TaskDisplayName;
        std::wstring Computer;
        std::wstring User;
        std::wstring ActivityId;
        std::wstring RelatedActivityId;
        std::uint32_t ProcessId{};
        std::uint32_t ThreadId{};
        std::wstring ShortDescription;
    };
}
