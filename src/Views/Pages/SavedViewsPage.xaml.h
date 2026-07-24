// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "SavedViewsPage.g.h"
#include "Core/Navigation/INavigationService.h"
#include "ViewModels/SavedViewsViewModel.h"

#include <winrt/Microsoft.UI.h>

#include <functional>
#include <memory>
#include <string_view>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace AstralChronicle::services
{
    struct ISavedViewRepository;
}

namespace winrt::AstralChronicle::implementation
{
    struct SavedViewsPage : SavedViewsPageT<SavedViewsPage>
    {
        SavedViewsPage();

        [[nodiscard]] winrt::AstralChronicle::SavedViewsViewModel ViewModel() const;
        void Initialize(
            std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> repository,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            ::AstralChronicle::navigation::INavigationService& navigation,
            std::function<void(std::wstring_view)> navigationSelectionChanged);

        void OnNewClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSaveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnOpenClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnPinClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnCopyClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnImportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnDuplicateClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnDeleteClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRefreshClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnTypeChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnChannelChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        winrt::fire_and_forget ExportAsync(winrt::hstring text, winrt::Microsoft::UI::WindowId windowId);
        winrt::fire_and_forget ImportAsync(winrt::Microsoft::UI::WindowId windowId);

        winrt::AstralChronicle::SavedViewsViewModel m_viewModel{ nullptr };
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        ::AstralChronicle::navigation::INavigationService* m_navigation{};
        std::function<void(std::wstring_view)> m_navigationSelectionChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SavedViewsPage : SavedViewsPageT<SavedViewsPage, implementation::SavedViewsPage>
    {
    };
}
