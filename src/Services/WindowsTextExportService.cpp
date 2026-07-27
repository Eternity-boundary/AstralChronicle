// Created by EternityBoundary on Jul 28, 2026
#include "pch.h"
#include "WindowsTextExportService.h"

#include <winrt/Microsoft.Windows.Storage.Pickers.h>

#include <filesystem>
#include <fstream>

namespace AstralChronicle::services
{
    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
        WindowsTextExportService::ExportTextAsync(
            winrt::Microsoft::UI::WindowId const& windowId,
            TextExportRequest const& request)
    {
        winrt::Microsoft::Windows::Storage::Pickers::FileSavePicker picker{ windowId };
        picker.SuggestedStartLocation(
            winrt::Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.SuggestedFileName(request.SuggestedFileName);
        picker.DefaultFileExtension(L".txt");

        auto const fileTypes = winrt::single_threaded_vector<winrt::hstring>();
        fileTypes.Append(L".txt");
        picker.FileTypeChoices().Insert(request.FileTypeDescription, fileTypes);

        auto const file = co_await picker.PickSaveFileAsync();
        if (!file)
        {
            co_return {};
        }

        auto const path = file.Path();
        co_await winrt::resume_background();
        auto const content = winrt::to_string(request.Text);
        std::ofstream output{
            std::filesystem::path{ path.c_str() },
            std::ios::binary | std::ios::trunc };
        if (!output)
        {
            throw winrt::hresult_error{ E_FAIL };
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output)
        {
            throw winrt::hresult_error{ E_FAIL };
        }
        co_return path;
    }
}
