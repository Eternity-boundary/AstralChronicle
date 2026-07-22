// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "RemotePage.g.h"
#include "ViewModels/RemoteViewModel.h"
namespace AstralChronicle::services { struct IRemoteEventService; struct IEventQueryService; }
namespace AstralChronicle::design { struct IStringResourceService; }
namespace winrt::AstralChronicle::implementation
{
    struct RemotePage : RemotePageT<RemotePage>
    {
        RemotePage();
        [[nodiscard]] winrt::AstralChronicle::RemoteViewModel ViewModel() const;
        void Initialize(::AstralChronicle::services::IRemoteEventService& service, ::AstralChronicle::services::IEventQueryService const& localQuery, ::AstralChronicle::design::IStringResourceService const& strings);
        void OnConnectClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnDisconnectClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSaveConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRemoveConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnTestConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRefreshChannelsClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRunQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSaveQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRemoveQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSavedQuerySelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnQueryChannelChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnCompareQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnStartRemoteLiveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnStopRemoteLiveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSavedConnectionSelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnPasswordChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
    private:
        winrt::AstralChronicle::RemoteViewModel m_viewModel{ nullptr };
    };
}
namespace winrt::AstralChronicle::factory_implementation { struct RemotePage : RemotePageT<RemotePage, implementation::RemotePage> { }; }
