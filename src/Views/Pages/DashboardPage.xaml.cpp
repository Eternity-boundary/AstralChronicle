// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardPage.xaml.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "DashboardPage.g.cpp"

#include <cwchar>
#include <string>
#include <utility>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.System.h>

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
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
        ::AstralChronicle::navigation::INavigationService& navigation,
        std::function<void(std::wstring_view)> navigationSelectionChanged)
    {
        m_navigation = &navigation;
        m_navigationSelectionChanged = std::move(navigationSelectionChanged);
        Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            DashboardLoadingIndicator(),
            strings->GetString(L"Dashboard.Loading.Text"));
        auto dispatcherForViewModel = PageRoot().DispatcherQueue();
        if (!dispatcherForViewModel)
        {
            dispatcherForViewModel = dispatcher;
        }
        winrt::get_self<DashboardViewModel>(m_viewModel)->Initialize(
            eventQuery,
            strings,
            dispatcherForViewModel);
    }

    void DashboardPage::OnNavigationCardTapped(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args)
    {
        args.Handled(NavigateFromCard(sender));
    }

    void DashboardPage::OnNavigationCardKeyDown(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        auto const key = args.Key();
        if (key != winrt::Windows::System::VirtualKey::Enter &&
            key != winrt::Windows::System::VirtualKey::Space)
        {
            return;
        }

        args.Handled(NavigateFromCard(sender));
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
            NavigateToEvent(item.Channel(), item.SortRecordId());
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
        request.Query = DashboardViewModel::QueryForTodayCriticalEvents();
        Navigate(request);
    }

    void DashboardPage::NavigateToEvent(
        winrt::hstring const& channel,
        std::uint64_t const recordId)
    {
        if (!m_navigation || channel.empty() || recordId == 0)
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
        request.Query = L"*[System[EventRecordID=" + std::to_wstring(recordId) + L"]]";
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

    bool DashboardPage::NavigateFromCard(
        winrt::Windows::Foundation::IInspectable const& sender)
    {
        auto const card = sender.try_as<Microsoft::UI::Xaml::Controls::ContentControl>();
        if (!card)
        {
            return false;
        }

        if (card == DashboardErrorNavigationCard())
        {
            NavigateToLevel(2);
            return true;
        }

        if (card == DashboardWarningNavigationCard())
        {
            NavigateToLevel(3);
            return true;
        }

        if (card == DashboardCriticalNavigationCard())
        {
            NavigateToLevel(1);
            return true;
        }

        return false;
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
        request.Query = DashboardViewModel::QueryForTodayLevel(level);
        Navigate(request);
    }
}
