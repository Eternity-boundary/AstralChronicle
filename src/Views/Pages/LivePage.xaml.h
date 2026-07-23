// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "LivePage.g.h"
#include "ViewModels/LiveViewModel.h"

namespace AstralChronicle::services { struct IEventLiveService; }
namespace AstralChronicle::design { struct IStringResourceService; }

namespace winrt::AstralChronicle::implementation
{
    struct LivePage : LivePageT<LivePage>
    {
        LivePage();
        [[nodiscard]] winrt::AstralChronicle::LiveViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventLiveService& liveService,
            ::AstralChronicle::design::IStringResourceService const& strings);
        void OnStartClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnPauseClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnResumeClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnStopClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnClearClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRecordClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnBookmarkClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnChannelChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    private:
        winrt::AstralChronicle::LiveViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct LivePage : LivePageT<LivePage, implementation::LivePage> { };
}
