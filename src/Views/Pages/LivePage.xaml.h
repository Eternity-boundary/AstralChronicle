// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "LivePage.g.h"
#include "ViewModels/LiveViewModel.h"

#include <memory>

namespace AstralChronicle::services { struct IEventLiveService; }
namespace AstralChronicle::design { struct IStringResourceService; }

namespace winrt::AstralChronicle::implementation
{
    struct LivePage : LivePageT<LivePage>
    {
        LivePage();
        [[nodiscard]] winrt::AstralChronicle::LiveViewModel ViewModel() const;
        void Initialize(
            std::shared_ptr<::AstralChronicle::services::IEventLiveService> liveService,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings);
        void OnStartClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnPauseClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnResumeClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnStopClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnClearClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRecordClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnBookmarkClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnChannelChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnUnloaded(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
    private:
        winrt::AstralChronicle::LiveViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct LivePage : LivePageT<LivePage, implementation::LivePage> { };
}
