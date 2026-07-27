// Created by EternityBoundary on Jul 28, 2026
#pragma once

#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.h>

namespace AstralChronicle::services
{
    struct TextExportRequest final
    {
        winrt::hstring Text;
        winrt::hstring SuggestedFileName;
        winrt::hstring FileTypeDescription;
    };

    struct ITextExportService
    {
        virtual ~ITextExportService() = default;

        // An empty path represents a cancelled picker. Failures propagate as hresult_error.
        [[nodiscard]] virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
            ExportTextAsync(
                winrt::Microsoft::UI::WindowId const& windowId,
                TextExportRequest const& request) = 0;
    };
}
