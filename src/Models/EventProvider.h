// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AstralChronicle::models
{
    struct EventProviderSummary final
    {
        std::wstring Name;
    };

    struct EventProviderEventDefinition final
    {
        std::uint32_t EventId{};
        std::uint32_t Version{};
        std::uint32_t Channel{};
        std::uint32_t Level{};
        std::uint32_t Opcode{};
        std::uint32_t Task{};
        std::uint64_t Keyword{};
        std::wstring Template;
    };

    struct EventProviderDetails final
    {
        std::wstring Name;
        std::wstring Guid;
        std::wstring ResourceFilePath;
        std::wstring ParameterFilePath;
        std::wstring MessageFilePath;
        std::wstring HelpLink;
        std::vector<EventProviderEventDefinition> EventDefinitions;
        std::uint32_t ErrorCode{};
        bool MetadataAvailable{};
    };
}
