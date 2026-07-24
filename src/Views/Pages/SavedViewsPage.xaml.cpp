// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SavedViewsPage.xaml.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "SavedViewsPage.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace winrt::AstralChronicle::implementation
{
    SavedViewsPage::SavedViewsPage()
        : m_viewModel(winrt::make<SavedViewsViewModel>())
    {
        InitializeComponent();
    }

    winrt::AstralChronicle::SavedViewsViewModel SavedViewsPage::ViewModel() const
    {
        return m_viewModel;
    }

    void SavedViewsPage::Initialize(
        std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> repository,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        ::AstralChronicle::navigation::INavigationService& navigation,
        std::function<void(std::wstring_view)> navigationSelectionChanged)
    {
        m_strings = strings;
        m_navigation = &navigation;
        m_navigationSelectionChanged = std::move(navigationSelectionChanged);
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Initialize(
            std::move(repository),
            std::move(strings),
            PageRoot().DispatcherQueue());
    }

    void SavedViewsPage::OnNewClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->NewView();
    }

    void SavedViewsPage::OnSaveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Save();
    }

    void SavedViewsPage::OnOpenClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const viewModel = winrt::get_self<SavedViewsViewModel>(m_viewModel);
        if (!m_navigation || !viewModel->HasSelection()) return;
        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Channel = ::AstralChronicle::models::EventChannelIdentifier{ std::wstring{ viewModel->EditorChannel().c_str() } };
        request.Query = std::wstring{ viewModel->EditorQuery().c_str() };
        if (m_navigation->Navigate(request) && m_navigationSelectionChanged)
        {
            m_navigationSelectionChanged(request.Route);
        }
    }

    void SavedViewsPage::OnPinClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->TogglePin();
    }

    void SavedViewsPage::OnCopyClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const query = winrt::get_self<SavedViewsViewModel>(m_viewModel)->CopyQuery();
        if (query.empty()) return;
        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(query);
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
    }

    void SavedViewsPage::OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const text = winrt::get_self<SavedViewsViewModel>(m_viewModel)->ExportText();
        if (!text.empty())
        {
            ExportAsync(text, PageRoot().XamlRoot().ContentIslandEnvironment().AppWindowId());
        }
    }

    void SavedViewsPage::OnImportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ImportAsync(PageRoot().XamlRoot().ContentIslandEnvironment().AppWindowId());
    }

    void SavedViewsPage::OnDuplicateClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Duplicate();
    }

    void SavedViewsPage::OnDeleteClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Delete();
    }

    void SavedViewsPage::OnRefreshClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Refresh();
    }

    void SavedViewsPage::OnTypeChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        if (!combo.SelectedValue()) return;
        try
        {
            winrt::get_self<SavedViewsViewModel>(m_viewModel)->EditorType(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
        }
    }

    void SavedViewsPage::OnChannelChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        if (!combo.SelectedValue()) return;
        try
        {
            winrt::get_self<SavedViewsViewModel>(m_viewModel)->EditorChannel(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
        }
    }

    winrt::fire_and_forget SavedViewsPage::ExportAsync(
        winrt::hstring text,
        winrt::Microsoft::UI::WindowId windowId)
    {
        try
        {
            auto lifetime = get_strong();
            auto const strings = m_strings;
            Microsoft::Windows::Storage::Pickers::FileSavePicker picker{ windowId };
            picker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.SuggestedFileName(strings ? strings->GetString(L"SavedViews.ExportSuggestedFileName.Text") : winrt::hstring{ L"SavedViews-export" });
            picker.DefaultFileExtension(L".txt");
            auto const fileTypes = winrt::single_threaded_vector<winrt::hstring>();
            fileTypes.Append(L".txt");
            picker.FileTypeChoices().Insert(
                strings ? strings->GetString(L"SavedViews.ExportTextFiles.Text") : winrt::hstring{ L"Text files (*.txt)" },
                fileTypes);
            auto const result = co_await picker.PickSaveFileAsync();
            if (result)
            {
                auto const path = result.Path();
                co_await winrt::resume_background();
                auto const content = winrt::to_string(text);
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
            }
        }
        catch (...)
        {
        }
    }

    winrt::fire_and_forget SavedViewsPage::ImportAsync(winrt::Microsoft::UI::WindowId windowId)
    {
        try
        {
            auto lifetime = get_strong();
            auto const dispatcher = PageRoot().DispatcherQueue();
            Microsoft::Windows::Storage::Pickers::FileOpenPicker picker{ windowId };
            picker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.FileTypeFilter().Append(L".txt");
            auto const result = co_await picker.PickSingleFileAsync();
            if (!result)
            {
                co_return;
            }

            auto const path = result.Path();
            co_await winrt::resume_background();
            std::ifstream input{
                std::filesystem::path{ path.c_str() },
                std::ios::binary | std::ios::ate };
            if (!input)
            {
                throw winrt::hresult_error{ E_FAIL };
            }

            constexpr std::streamoff maxImportBytes = 4 * 1024 * 1024;
            auto const size = input.tellg();
            if (size <= 0 || size > maxImportBytes)
            {
                co_return;
            }

            input.seekg(0, std::ios::beg);
            std::string content(static_cast<std::size_t>(size), '\0');
            input.read(content.data(), static_cast<std::streamsize>(content.size()));
            if (!input)
            {
                throw winrt::hresult_error{ E_FAIL };
            }

            if (content.size() >= 3 &&
                static_cast<unsigned char>(content[0]) == 0xEF &&
                static_cast<unsigned char>(content[1]) == 0xBB &&
                static_cast<unsigned char>(content[2]) == 0xBF)
            {
                content.erase(0, 3);
            }
            if (content.size() >= 2 &&
                ((static_cast<unsigned char>(content[0]) == 0xFF && static_cast<unsigned char>(content[1]) == 0xFE) ||
                    (static_cast<unsigned char>(content[0]) == 0xFE && static_cast<unsigned char>(content[1]) == 0xFF)))
            {
                co_return;
            }

            auto const decoded = winrt::to_hstring(content);
            co_await wil::resume_foreground(dispatcher);
            if (!decoded.empty())
            {
                winrt::get_self<SavedViewsViewModel>(m_viewModel)->ImportText(decoded);
            }
        }
        catch (...)
        {
        }
    }
}
