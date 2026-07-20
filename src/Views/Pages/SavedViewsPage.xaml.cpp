// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SavedViewsPage.xaml.h"

#include "SavedViewsPage.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>

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
        ::AstralChronicle::services::ISavedViewRepository& repository,
        ::AstralChronicle::design::IStringResourceService const& strings,
        ::AstralChronicle::navigation::INavigationService& navigation)
    {
        m_navigation = &navigation;
        winrt::get_self<SavedViewsViewModel>(m_viewModel)->Initialize(repository, strings, PageRoot().DispatcherQueue());
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
        (void)m_navigation->Navigate(request);
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
        if (!text.empty()) ExportAsync(text);
    }

    void SavedViewsPage::OnImportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ImportAsync();
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

    winrt::fire_and_forget SavedViewsPage::ExportAsync(winrt::hstring text)
    {
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        try
        {
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = co_await folder.CreateFileAsync(
                L"SavedViews-export.txt",
                winrt::Windows::Storage::CreationCollisionOption::ReplaceExisting);
            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, text);
        }
        catch (...)
        {
        }
    }

    winrt::fire_and_forget SavedViewsPage::ImportAsync()
    {
        auto lifetime = get_strong();
        auto const dispatcher = PageRoot().DispatcherQueue();
        winrt::hstring text;
        co_await winrt::resume_background();
        try
        {
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = co_await folder.TryGetItemAsync(L"SavedViews-import.txt");
            if (file)
            {
                text = co_await winrt::Windows::Storage::FileIO::ReadTextAsync(file.as<winrt::Windows::Storage::StorageFile>());
            }
        }
        catch (...)
        {
        }
        co_await wil::resume_foreground(dispatcher);
        if (!text.empty()) winrt::get_self<SavedViewsViewModel>(m_viewModel)->ImportText(text);
    }
}
