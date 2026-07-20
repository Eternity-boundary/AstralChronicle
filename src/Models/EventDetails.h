// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "EventRecordSummary.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AstralChronicle::models
{
    struct EventProperty final
    {
        std::wstring Name;
        std::wstring Value;
    };

    struct ProviderMetadataSnapshot final
    {
        std::wstring Name;
        std::wstring Guid;
        std::wstring ResourceFilePath;
        std::wstring MessageFilePath;
        std::wstring HelpLink;
        bool IsAvailable{};
    };

    struct EventDetails final
    {
        EventRecordSummary Summary;
        std::wstring FormattedMessage;
        std::wstring RawXml;
        std::vector<EventProperty> SystemProperties;
        std::vector<EventProperty> EventData;
        std::vector<EventProperty> UserData;
        std::vector<std::wstring> Keywords;
        std::wstring BinaryData;
        std::wstring RelatedEvents;
        ProviderMetadataSnapshot ProviderMetadata;
        std::uint32_t FormattingErrorCode{};
    };
}
