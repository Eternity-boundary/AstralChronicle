// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "RemotePage.xaml.h"
#include "RemotePage.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    RemotePage::RemotePage() : m_viewModel(winrt::make<RemoteViewModel>()) { InitializeComponent(); }
    winrt::AstralChronicle::RemoteViewModel RemotePage::ViewModel() const { return m_viewModel; }
    void RemotePage::Initialize(::AstralChronicle::services::IRemoteEventService& service, ::AstralChronicle::services::IEventQueryService const& localQuery, ::AstralChronicle::design::IStringResourceService const& strings) { winrt::get_self<RemoteViewModel>(m_viewModel)->Initialize(service, localQuery, strings, PageRoot().DispatcherQueue()); }
    void RemotePage::OnConnectClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->Connect(); }
    void RemotePage::OnDisconnectClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->Disconnect(); }
    void RemotePage::OnSaveConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->SaveConnection(); }
    void RemotePage::OnRemoveConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->RemoveConnection(); }
    void RemotePage::OnTestConnectionClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->TestConnection(); }
    void RemotePage::OnRefreshChannelsClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->RefreshChannels(); }
    void RemotePage::OnRunQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->RunQuery(); }
    void RemotePage::OnSaveQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->SaveQuery(); }
    void RemotePage::OnRemoveQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->RemoveQuery(); }
    void RemotePage::OnSavedQuerySelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&) { auto list = sender.as<Microsoft::UI::Xaml::Controls::ListView>(); winrt::get_self<RemoteViewModel>(m_viewModel)->SelectSavedQuery(list.SelectedIndex()); }
    void RemotePage::OnCompareQueryClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->CompareQuery(); }
    void RemotePage::OnStartRemoteLiveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->StartRemoteLive(); }
    void RemotePage::OnStopRemoteLiveClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->StopRemoteLive(); }
    void RemotePage::OnSavedConnectionSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&) { auto list = sender.as<Microsoft::UI::Xaml::Controls::ListView>(); winrt::get_self<RemoteViewModel>(m_viewModel)->SelectSavedConnection(list.SelectedIndex()); }
    void RemotePage::OnPasswordChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<RemoteViewModel>(m_viewModel)->Password(sender.as<Microsoft::UI::Xaml::Controls::PasswordBox>().Password()); }
}
