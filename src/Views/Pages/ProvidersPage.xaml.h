// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "ProvidersPage.g.h"
#include "Core/Navigation/INavigationService.h"
#include "ViewModels/ProvidersViewModel.h"

#include <functional>
#include <string_view>

namespace AstralChronicle::services
{
    struct IEventProviderService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct ProvidersPage : ProvidersPageT<ProvidersPage>
    {
        ProvidersPage();

        [[nodiscard]] winrt::AstralChronicle::ProvidersViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventProviderService const& providerService,
            ::AstralChronicle::design::IStringResourceService const& strings,
            ::AstralChronicle::navigation::INavigationService& navigation,
            std::function<void(std::wstring_view)> navigationSelectionChanged);

        void OnRefreshClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSearchTextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);
        void OnCopyClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnOpenEventsClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnExportClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        [[nodiscard]] winrt::hstring BuildCopyText() const;
        winrt::fire_and_forget ExportAsync(winrt::hstring text);

        winrt::AstralChronicle::ProvidersViewModel m_viewModel{ nullptr };
        ::AstralChronicle::navigation::INavigationService* m_navigation{};
        std::function<void(std::wstring_view)> m_navigationSelectionChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct ProvidersPage : ProvidersPageT<ProvidersPage, implementation::ProvidersPage>
    {
    };
}
