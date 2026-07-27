// Created by EternityBoundary on Jul 27,2026
#pragma once

#include "Models/EventDetails.h"
#include "Models/LiveEventRecord.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AstralChronicle::services
{
    struct LiveEventDetailsText final
    {
        std::wstring Message;
        std::wstring RawXml;
        std::wstring EventData;
        std::wstring UserData;
        std::wstring ProviderMetadata;
        std::wstring BinaryData;
        std::wstring RelatedEvents;
    };

    struct LiveEventExportResult final
    {
        std::wstring Path;
        std::uint32_t ErrorCode{};
    };

    // Owns raw live-event payload transformation and recording export. These
    // operations are deliberately kept out of the ViewModel so it only manages
    // UI state and invokes services.
    struct IEventLiveDataService
    {
        virtual ~IEventLiveDataService() = default;

        [[nodiscard]] virtual models::LiveEventRecord CreateRecord(std::wstring rawXml) const = 0;
        [[nodiscard]] virtual models::EventDetails CreateFallbackDetails(
            models::LiveEventRecord const& record) const = 0;
        [[nodiscard]] virtual LiveEventDetailsText CreateDetailsText(
            models::EventDetails const& details) const = 0;
        [[nodiscard]] virtual LiveEventExportResult ExportRecordedEvents(
            std::vector<std::wstring> events,
            bool redactComputerName,
            bool redactUserNames) const = 0;
    };
}
