// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "TimelinePage.g.h"
#include "Core/Navigation/INavigationService.h"
#include "ViewModels/TimelineViewModel.h"

namespace AstralChronicle::services
{
    struct IEventQueryService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct TimelinePage : TimelinePageT<TimelinePage>
    {
        TimelinePage();

        [[nodiscard]] winrt::AstralChronicle::TimelineViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            ::AstralChronicle::navigation::INavigationService& navigation);
        void OnTimeRangeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnLevelFilterChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnChannelFilterChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnRefreshClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnOpenClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnBookmarkClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnExportClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnTimelineSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        void NavigateToEvent(winrt::AstralChronicle::EventLogItemViewModel const& item);
        winrt::fire_and_forget ExportAsync(winrt::hstring text);

        winrt::AstralChronicle::TimelineViewModel m_viewModel{ nullptr };
        ::AstralChronicle::navigation::INavigationService* m_navigation{};
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct TimelinePage : TimelinePageT<TimelinePage, implementation::TimelinePage>
    {
    };
}
