// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardPage.xaml.h"

#include "DashboardPage.g.cpp"

#include <cwchar>
#include <string>
#include <utility>
#include <winrt/Microsoft.UI.Dispatching.h>

namespace winrt::AstralChronicle::implementation
{
    DashboardPage::DashboardPage()
        : m_viewModel(winrt::make<DashboardViewModel>())
    {
        InitializeComponent();
    }

    winrt::AstralChronicle::DashboardViewModel DashboardPage::ViewModel() const
    {
        return m_viewModel;
    }

    void DashboardPage::Initialize(
        ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
        ::AstralChronicle::navigation::INavigationService& navigation,
        std::function<void(std::wstring_view)> navigationSelectionChanged)
    {
        m_navigation = &navigation;
        m_navigationSelectionChanged = std::move(navigationSelectionChanged);
        auto dispatcherForViewModel = PageRoot().DispatcherQueue();
        if (!dispatcherForViewModel)
        {
            dispatcherForViewModel = dispatcher;
        }
        winrt::get_self<DashboardViewModel>(m_viewModel)->Initialize(
            eventLogCatalog,
            eventQuery,
            strings,
            dispatcherForViewModel);
    }

    void DashboardPage::OnErrorCardTapped(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
    {
        NavigateToLevel(2);
    }

    void DashboardPage::OnWarningCardTapped(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
    {
        NavigateToLevel(3);
    }

    void DashboardPage::OnCriticalCardTapped(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
    {
        NavigateToLevel(1);
    }

    void DashboardPage::OnRecentEventClicked(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const button = sender.try_as<Microsoft::UI::Xaml::FrameworkElement>();
        if (!button)
        {
            return;
        }

        auto const item = button.DataContext().try_as<winrt::AstralChronicle::EventLogItemViewModel>();
        if (item)
        {
            NavigateToEvent(item.Channel(), item.RecordId());
        }
    }

    void DashboardPage::OnViewAllEventsClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (!m_navigation)
        {
            return;
        }

        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Channel = ::AstralChronicle::models::EventChannelIdentifier{ L"System" };
        request.Query = L"*[System[(Level=1 or Level=2)]]";
        Navigate(request);
    }

    void DashboardPage::NavigateToEvent(
        winrt::hstring const& channel,
        winrt::hstring const& recordId)
    {
        if (!m_navigation || channel.empty() || recordId.empty())
        {
            return;
        }

        auto const numericRecordId = std::wcstoull(recordId.c_str(), nullptr, 10);
        if (numericRecordId == 0)
        {
            return;
        }

        auto channelPath = std::wstring{ channel.c_str() };
        if (channelPath.empty() || channelPath == L"—")
        {
            channelPath = L"System";
        }

        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Channel = ::AstralChronicle::models::EventChannelIdentifier{ std::move(channelPath) };
        request.Query = L"*[System[EventRecordID=" + std::to_wstring(numericRecordId) + L"]]";
        Navigate(request);
    }

    bool DashboardPage::Navigate(::AstralChronicle::navigation::NavigationRequest const& request)
    {
        if (!m_navigation || !m_navigation->Navigate(request))
        {
            return false;
        }

        if (m_navigationSelectionChanged)
        {
            m_navigationSelectionChanged(request.Route);
        }

        return true;
    }

    void DashboardPage::NavigateToLevel(std::uint8_t const level)
    {
        if (!m_navigation)
        {
            return;
        }

        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Channel = ::AstralChronicle::models::EventChannelIdentifier{ L"System" };
        request.Query = L"*[System[Level=" + std::to_wstring(level) + L"]]";
        Navigate(request);
    }
}
