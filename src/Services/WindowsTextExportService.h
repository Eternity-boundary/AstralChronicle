// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include "ITextExportService.h"

namespace AstralChronicle::services
{
    class WindowsTextExportService final : public ITextExportService
    {
    public:
        [[nodiscard]] winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
            ExportTextAsync(
                winrt::Microsoft::UI::WindowId const& windowId,
                TextExportRequest const& request) override;
    };
}
