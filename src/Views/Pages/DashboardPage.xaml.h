// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DashboardPage.g.h"
#include "Core/Navigation/INavigationService.h"
#include "ViewModels/DashboardViewModel.h"

#include <winrt/Microsoft.UI.Xaml.Input.h>

#include <functional>
#include <string_view>

namespace AstralChronicle::services
{
    struct IEventLogCatalogService;
    struct IEventQueryService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage>
    {
        DashboardPage();

        [[nodiscard]] winrt::AstralChronicle::DashboardViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            ::AstralChronicle::navigation::INavigationService& navigation,
            std::function<void(std::wstring_view)> navigationSelectionChanged);
        void OnErrorCardTapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void OnWarningCardTapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void OnCriticalCardTapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void OnRecentEventClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnViewAllEventsClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        bool Navigate(::AstralChronicle::navigation::NavigationRequest const& request);
        void NavigateToLevel(std::uint8_t level);
        void NavigateToEvent(winrt::hstring const& channel, winrt::hstring const& recordId);
        winrt::AstralChronicle::DashboardViewModel m_viewModel{ nullptr };
        ::AstralChronicle::navigation::INavigationService* m_navigation{};
        std::function<void(std::wstring_view)> m_navigationSelectionChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage, implementation::DashboardPage>
    {
    };
}
