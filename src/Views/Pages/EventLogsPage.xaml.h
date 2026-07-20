// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "EventLogsPage.g.h"
#include "Models/EventChannelDescriptor.h"
#include "ViewModels/EventLogsViewModel.h"

#include <optional>
#include <vector>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace AstralChronicle::services
{
    struct IEventQueryService;
}

namespace winrt::AstralChronicle::implementation
{
    struct EventLogsPage : EventLogsPageT<EventLogsPage>
    {
        EventLogsPage();

        [[nodiscard]] winrt::AstralChronicle::EventLogsViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings,
            std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel = std::nullopt,
            std::optional<std::wstring> const& query = std::nullopt);
        void OnRefreshClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnClearFilterClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCopyClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSortClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnApplyStructuredFilterClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnClearStructuredFilterClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnFilterLevelChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnBookmarkClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnExportClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnToggleDetailsClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnContentGridSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        void UpdateResponsiveLayout(double width);

        winrt::AstralChronicle::EventLogsViewModel m_viewModel{ nullptr };
        bool m_detailsPaneVisible{ true };
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct EventLogsPage : EventLogsPageT<EventLogsPage, implementation::EventLogsPage>
    {
    };
}
