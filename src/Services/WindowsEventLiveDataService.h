// Created by EternityBoundary on Jul 27,2026
#pragma once

#include "IEventLiveDataService.h"

namespace AstralChronicle::services
{
    class WindowsEventLiveDataService final : public IEventLiveDataService
    {
    public:
        [[nodiscard]] models::LiveEventRecord CreateRecord(std::wstring rawXml) const override;
        [[nodiscard]] models::EventDetails CreateFallbackDetails(
            models::LiveEventRecord const& record) const override;
        [[nodiscard]] LiveEventDetailsText CreateDetailsText(
            models::EventDetails const& details) const override;
        [[nodiscard]] LiveEventExportResult ExportRecordedEvents(
            std::vector<std::wstring> events,
            bool redactComputerName,
            bool redactUserNames) const override;
    };
}
