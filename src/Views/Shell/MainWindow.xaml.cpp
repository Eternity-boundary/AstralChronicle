// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "MainWindow.xaml.h"
#include "App/AppHost.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/IEventLogCatalogService.h"
#include "Views/Pages/DashboardPage.xaml.h"
#include "Views/Pages/EventLogsPage.xaml.h"
#include "Views/Pages/LivePage.xaml.h"
#include "Views/Pages/ProvidersPage.xaml.h"
#include "Views/Pages/RemotePage.xaml.h"
#include "Views/Pages/SavedViewsPage.xaml.h"
#include "Views/Pages/SessionsPage.xaml.h"
#include "Views/Pages/SettingsPage.xaml.h"
#include "Views/Pages/TimelinePage.xaml.h"

#include "MainWindow.g.cpp"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <string_view>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::AstralChronicle::implementation
{
    namespace
    {
        using NavigationViewItem = Microsoft::UI::Xaml::Controls::NavigationViewItem;

        constexpr std::wstring_view ChannelTagPrefix{ L"channel:" };
        constexpr std::wstring_view LandingTagPrefix{ L"landing:" };
        constexpr std::wstring_view DynamicGroupTagPrefix{ L"dynamic-group:" };
        constexpr std::wstring_view ApplicationsAndServicesTag{ L"landing:event-logs/applications-services" };

        [[nodiscard]] std::wstring ToWString(hstring const& value)
        {
            return { value.c_str(), value.size() };
        }

        [[nodiscard]] std::wstring TagOf(NavigationViewItem const& item)
        {
            try
            {
                return ToWString(unbox_value<hstring>(item.Tag()));
            }
            catch (...)
            {
                return {};
            }
        }

        [[nodiscard]] bool HasPrefix(std::wstring_view value, std::wstring_view prefix)
        {
            return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
        }

        hstring ShellGreetingResourceKeyForCurrentLocalTime()
        {
            auto const now = std::time(nullptr);
            std::tm localTime{};
            if (localtime_s(&localTime, &now) != 0)
            {
                return L"ShellGreeting.Morning";
            }

            auto const hour = localTime.tm_hour;
            if (hour >= 5 && hour < 12)
            {
                return L"ShellGreeting.Morning";
            }

            if (hour < 18)
            {
                return L"ShellGreeting.Afternoon";
            }

            if (hour < 22)
            {
                return L"ShellGreeting.Evening";
            }

            return L"ShellGreeting.Night";
        }
    }

    MainWindow::~MainWindow()
    {
        if (m_greetingTimer)
        {
            m_greetingTimer.Stop();
        }

        if (m_theme && m_themeSubscriptionId != 0)
        {
            m_theme->Unsubscribe(m_themeSubscriptionId);
        }
    }

    void MainWindow::Initialize(::AstralChronicle::app::AppHost& host)
    {
        InitializeComponent();

        try
        {
            SystemBackdrop(Microsoft::UI::Xaml::Media::MicaBackdrop{});
        }
        catch (...)
        {
            // Some remote or restricted desktop sessions do not support Mica.
        }
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        Title(host.Strings().GetString(L"MainWindow.Title"));
        m_strings = &host.Strings();
        m_eventLogCatalog = &host.EventLogCatalog();
        UpdateShellGreeting();
        m_greetingTimer = RootLayout().DispatcherQueue().CreateTimer();
        m_greetingTimer.Interval(std::chrono::minutes{ 1 });
        m_greetingTimer.Tick([this](auto const&, auto const&)
            {
                UpdateShellGreeting();
            });
        m_greetingTimer.Start();
        host.Theme().Initialize(RootLayout());
        m_theme = &host.Theme();
        m_themeSubscriptionId = m_theme->Subscribe([this](::AstralChronicle::design::ThemeMode)
            {
                ApplyThemeBackdrop();
            });
        ApplyThemeBackdrop();

        m_navigation = &host.Navigation();
        m_navigation->Attach(ContentFrame());
        m_navigation->Register({
            L"dashboard",
            [this, &host]()
            {
                auto page = make<DashboardPage>();
                get_self<DashboardPage>(page)->Initialize(
                    host.EventLogCatalog(),
                    host.EventQuery(),
                    host.Strings(),
                    RootLayout().DispatcherQueue(),
                    host.Navigation());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"event-logs",
            [&host]()
            {
                auto page = make<EventLogsPage>();
                get_self<EventLogsPage>(page)->Initialize(host.EventQuery(), host.Strings());
                return page.as<FrameworkElement>();
            },
            [&host](::AstralChronicle::navigation::NavigationRequest const& request)
            {
                auto page = make<EventLogsPage>();
                get_self<EventLogsPage>(page)->Initialize(
                    host.EventQuery(),
                    host.Strings(),
                    request.Channel,
                    request.Query);
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"timeline",
            [this, &host]()
            {
                auto page = make<TimelinePage>();
                get_self<TimelinePage>(page)->Initialize(
                    host.EventQuery(),
                    host.Strings(),
                    RootLayout().DispatcherQueue(),
                    host.Navigation());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"providers",
            [this, &host]()
            {
                auto page = make<ProvidersPage>();
                get_self<ProvidersPage>(page)->Initialize(
                    host.EventProviders(),
                    host.Strings(),
                    host.Navigation());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"sessions",
            [this, &host]()
            {
                auto page = make<SessionsPage>();
                get_self<SessionsPage>(page)->Initialize(host.Sessions(), host.Strings());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"saved-views",
            [this, &host]()
            {
                auto page = make<SavedViewsPage>();
                get_self<SavedViewsPage>(page)->Initialize(
                    host.SavedViews(),
                    host.Strings(),
                    host.Navigation());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"live",
            [this, &host]()
            {
                auto page = make<LivePage>();
                get_self<LivePage>(page)->Initialize(host.EventLive(), host.Strings());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"remote",
            [this, &host]()
            {
                auto page = make<RemotePage>();
                get_self<RemotePage>(page)->Initialize(host.RemoteEvents(), host.EventQuery(), host.Strings());
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"settings",
            [&host]
            {
                auto page = make<SettingsPage>();
                get_self<SettingsPage>(page)->Initialize(host.Theme(), host.Strings());
                return page.as<FrameworkElement>();
            } });

        RootNavigationView().SelectedItem(RootNavigationView().MenuItems().GetAt(0));
        RootNavigationView().IsPaneOpen(true);
        UpdateThemeBackdropLayout();
    }

    void MainWindow::ApplyThemeBackdrop()
    {
        if (!m_theme)
        {
            return;
        }

        auto const backdrop = m_theme->Backdrop();
        auto apply = [](Microsoft::UI::Xaml::Controls::Border const& target,
                        Microsoft::UI::Xaml::Media::Brush const& brush)
        {
            target.Background(brush);
            target.Visibility(brush ? Visibility::Visible : Visibility::Collapsed);
        };

        apply(ThemeMainBackground(), backdrop.MainImage);
        apply(ThemeMainContrastOverlay(), backdrop.MainContrastOverlay);
        apply(ThemeNavigationBackground(), backdrop.NavigationImage);
        apply(ThemeNavigationContrastOverlay(), backdrop.NavigationContrastOverlay);
        UpdateThemeBackdropLayout();
    }

    void MainWindow::UpdateShellGreeting()
    {
        if (!m_strings)
        {
            return;
        }

        ShellGreetingText().Text(m_strings->GetString(ShellGreetingResourceKeyForCurrentLocalTime()));
    }

    void MainWindow::UpdateThemeBackdropLayout()
    {
        auto const paneWidth = NavigationPaneWidth();
        auto const contentMargin = Thickness{ paneWidth, 0.0, 0.0, 0.0 };

        ThemeNavigationBackground().Width(paneWidth);
        ThemeNavigationContrastOverlay().Width(paneWidth);
        ThemeMainBackground().Margin(contentMargin);
        ThemeMainContrastOverlay().Margin(contentMargin);
    }

    double MainWindow::NavigationPaneWidth()
    {
        auto const navigationView = RootNavigationView();
        if (navigationView.PaneDisplayMode() == Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::Left)
        {
            return navigationView.IsPaneOpen()
                ? navigationView.OpenPaneLength()
                : 0.0;
        }

        return navigationView.IsPaneOpen()
            ? navigationView.OpenPaneLength()
            : navigationView.CompactPaneLength();
    }

    void MainWindow::OnNavigationViewLoaded(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        RootNavigationView().IsPaneOpen(true);
        RestoreNavigationExpansionState();
        UpdateThemeBackdropLayout();
    }

    void MainWindow::OnNavigationPaneOpened(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Windows::Foundation::IInspectable const&)
    {
        UpdateThemeBackdropLayout();
    }

    void MainWindow::OnNavigationPaneClosed(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Windows::Foundation::IInspectable const&)
    {
        UpdateThemeBackdropLayout();
    }

    bool MainWindow::IsExpansionStateStored(NavigationViewItem const& item) const
    {
        auto const tag = TagOf(item);
        if (tag.empty())
        {
            return false;
        }

        try
        {
            auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            std::wstring key{ L"EventLogs.Navigation.Expansion." };
            key += tag;
            if (!values.HasKey(hstring{ key }))
            {
                return false;
            }

            return unbox_value<bool>(values.Lookup(hstring{ key }));
        }
        catch (...)
        {
            // Unpackaged/design-time hosts may not expose LocalSettings.
            return false;
        }
    }

    void MainWindow::StoreExpansionState(NavigationViewItem const& item, bool const isExpanded) const
    {
        auto const tag = TagOf(item);
        if (tag.empty())
        {
            return;
        }

        try
        {
            auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            std::wstring key{ L"EventLogs.Navigation.Expansion." };
            key += tag;
            values.Insert(hstring{ key }, box_value(isExpanded));
        }
        catch (...)
        {
            // Expansion persistence is best effort and must not affect navigation.
        }
    }

    void MainWindow::RestoreNavigationExpansionState()
    {
        if (m_expansionStateRestored)
        {
            return;
        }

        m_expansionStateRestored = true;
        auto const menuItems = RootNavigationView().MenuItems();
        if (menuItems.Size() < 2)
        {
            return;
        }

        auto const eventLogs = menuItems.GetAt(1).as<NavigationViewItem>();
        auto const eventLogItems = eventLogs.MenuItems();
        m_expansionStateRestoring = true;
        if (IsExpansionStateStored(eventLogs))
        {
            eventLogs.IsExpanded(true);
        }

        for (std::uint32_t index{}; index < eventLogItems.Size(); ++index)
        {
            auto const item = eventLogItems.GetAt(index).as<NavigationViewItem>();
            if (IsExpansionStateStored(item))
            {
                item.IsExpanded(true);
            }

            auto const children = item.MenuItems();
            for (std::uint32_t childIndex{}; childIndex < children.Size(); ++childIndex)
            {
                auto const child = children.GetAt(childIndex).as<NavigationViewItem>();
                if (IsExpansionStateStored(child))
                {
                    child.IsExpanded(true);
                }
            }
        }
        m_expansionStateRestoring = false;
    }

    void MainWindow::StartDynamicChannelLoad()
    {
        if (m_dynamicChannelLoadRequested || m_dynamicChannelTreeLoaded || !m_eventLogCatalog)
        {
            return;
        }

        m_dynamicChannelLoadRequested = true;
        LoadDynamicChannelsAsync();
    }

    winrt::fire_and_forget MainWindow::LoadDynamicChannelsAsync()
    {
        auto lifetime = get_strong();
        auto const dispatcher = RootLayout().DispatcherQueue();
        std::optional<::AstralChronicle::services::ChannelPathTreeNode> tree;
        bool failed{};

        try
        {
            co_await winrt::resume_background();

            auto const channels = m_eventLogCatalog->EnumerateChannels();
            std::vector<::AstralChronicle::models::EventChannelDescriptor> dynamicChannels;
            dynamicChannels.reserve(channels.size());

            constexpr std::array<std::wstring_view, 5> WindowsLogPaths{
                L"Application",
                L"Security",
                L"Setup",
                L"System",
                L"ForwardedEvents" };

            for (auto const& channel : channels)
            {
                if (std::find(WindowsLogPaths.begin(), WindowsLogPaths.end(), channel.Path) == WindowsLogPaths.end())
                {
                    dynamicChannels.emplace_back(channel);
                }
            }

            tree.emplace();
            tree->Children = ::AstralChronicle::services::BuildChannelPathTree(dynamicChannels);
        }
        catch (...)
        {
            failed = true;
        }

        co_await wil::resume_foreground(dispatcher);
        if (failed || !tree)
        {
            m_dynamicChannelLoadRequested = false;
            if (m_dynamicChannelRoot)
            {
                NavigationViewItem unavailable;
                unavailable.Content(box_value(m_strings->GetString(L"EventLogs.DynamicChannelsUnavailable.Text")));
                unavailable.IsEnabled(false);
                m_dynamicChannelRoot.MenuItems().Append(unavailable);
                m_dynamicChannelRoot.HasUnrealizedChildren(false);
            }
            co_return;
        }

        m_dynamicChannelTree = std::move(tree);
        m_dynamicChannelIndex.clear();
        m_dynamicChannelGroupIdentifiers.clear();
        m_populatedDynamicPaths.clear();
        for (auto& node : m_dynamicChannelTree->Children)
        {
            IndexDynamicChannelNodes(node);
        }

        m_dynamicChannelTreeLoaded = true;
        if (!m_dynamicChannelRoot)
        {
            co_return;
        }

        if (m_dynamicChannelTree->Children.empty())
        {
            NavigationViewItem noChannels;
            noChannels.Content(box_value(m_strings->GetString(L"EventLogs.DynamicChannelsNone.Text")));
            noChannels.IsEnabled(false);
            m_dynamicChannelRoot.MenuItems().Append(noChannels);
            m_dynamicChannelRoot.HasUnrealizedChildren(false);
            co_return;
        }

        PopulateDynamicChildren(m_dynamicChannelRoot, {});
        RestoreDynamicExpansion(m_dynamicChannelRoot, {});
    }

    void MainWindow::IndexDynamicChannelNodes(::AstralChronicle::services::ChannelPathTreeNode& node)
    {
        if (!node.FullPath.empty())
        {
            m_dynamicChannelIndex.insert_or_assign(node.FullPath, &node);
        }

        for (auto& child : node.Children)
        {
            IndexDynamicChannelNodes(child);
        }
    }

    void MainWindow::PopulateDynamicChildren(NavigationViewItem const& parent, std::wstring_view parentPath)
    {
        auto const key = std::wstring{ parentPath };
        if (m_populatedDynamicPaths.find(key) != m_populatedDynamicPaths.end())
        {
            return;
        }

        std::vector<::AstralChronicle::services::ChannelPathTreeNode> const* children{};
        if (parentPath.empty())
        {
            if (!m_dynamicChannelTree)
            {
                return;
            }
            children = &m_dynamicChannelTree->Children;
        }
        else
        {
            auto const node = m_dynamicChannelIndex.find(key);
            if (node == m_dynamicChannelIndex.end())
            {
                return;
            }
            children = &node->second->Children;
        }

        for (auto const& node : *children)
        {
            NavigationViewItem item;
            std::wstring label = node.Segment;
            std::wstring tag;

            if (node.Channel && node.Children.empty())
            {
                tag = std::wstring{ ChannelTagPrefix } + node.Channel->Path;
                switch (node.Channel->State)
                {
                case ::AstralChronicle::models::EventChannelState::Disabled:
                    label += ToWString(m_strings->GetString(L"EventLogs.ChannelDisabledSuffix.Text"));
                    break;
                case ::AstralChronicle::models::EventChannelState::AccessDenied:
                    label += ToWString(m_strings->GetString(L"EventLogs.ChannelAccessDeniedSuffix.Text"));
                    break;
                case ::AstralChronicle::models::EventChannelState::Unavailable:
                    label += ToWString(m_strings->GetString(L"EventLogs.ChannelUnavailableSuffix.Text"));
                    break;
                case ::AstralChronicle::models::EventChannelState::Available:
                    break;
                }
            }
            else
            {
                tag = std::wstring{ DynamicGroupTagPrefix } + node.FullPath;
                if (node.Channel)
                {
                    m_dynamicChannelGroupIdentifiers.insert_or_assign(node.FullPath, node.Channel->Path);
                    switch (node.Channel->State)
                    {
                    case ::AstralChronicle::models::EventChannelState::Disabled:
                        label += ToWString(m_strings->GetString(L"EventLogs.ChannelDisabledSuffix.Text"));
                        break;
                    case ::AstralChronicle::models::EventChannelState::AccessDenied:
                        label += ToWString(m_strings->GetString(L"EventLogs.ChannelAccessDeniedSuffix.Text"));
                        break;
                    case ::AstralChronicle::models::EventChannelState::Unavailable:
                        label += ToWString(m_strings->GetString(L"EventLogs.ChannelUnavailableSuffix.Text"));
                        break;
                    case ::AstralChronicle::models::EventChannelState::Available:
                        break;
                    }
                }
                else
                {
                    item.SelectsOnInvoked(false);
                }
            }

            item.Content(box_value(hstring{ label }));
            item.Tag(box_value(hstring{ tag }));
            item.HasUnrealizedChildren(!node.Children.empty());
            parent.MenuItems().Append(item);
        }

        parent.HasUnrealizedChildren(false);
        m_populatedDynamicPaths.insert(key);
    }

    void MainWindow::RestoreDynamicExpansion(NavigationViewItem const& parent, std::wstring_view parentPath)
    {
        auto const key = std::wstring{ parentPath };
        std::vector<::AstralChronicle::services::ChannelPathTreeNode> const* children{};
        if (parentPath.empty())
        {
            if (!m_dynamicChannelTree)
            {
                return;
            }
            children = &m_dynamicChannelTree->Children;
        }
        else
        {
            auto const node = m_dynamicChannelIndex.find(key);
            if (node == m_dynamicChannelIndex.end())
            {
                return;
            }
            children = &node->second->Children;
        }

        for (std::uint32_t index{}; index < parent.MenuItems().Size() && index < children->size(); ++index)
        {
            auto const item = parent.MenuItems().GetAt(index).as<NavigationViewItem>();
            if (!IsExpansionStateStored(item))
            {
                continue;
            }

            auto const& node = (*children)[index];
            if (node.Children.empty())
            {
                continue;
            }

            PopulateDynamicChildren(item, node.FullPath);
            item.IsExpanded(true);
            RestoreDynamicExpansion(item, node.FullPath);
        }
    }

    void MainWindow::OnNavigationItemExpanding(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewItemExpandingEventArgs const& args)
    {
        auto const item = args.ExpandingItemContainer().try_as<NavigationViewItem>();
        if (!item)
        {
            return;
        }

        if (!m_expansionStateRestoring)
        {
            StoreExpansionState(item, true);
        }

        auto const tag = TagOf(item);
        if (tag == ApplicationsAndServicesTag)
        {
            m_dynamicChannelRoot = item;
            StartDynamicChannelLoad();
            return;
        }

        if (HasPrefix(tag, DynamicGroupTagPrefix))
        {
            PopulateDynamicChildren(item, std::wstring_view{ tag }.substr(DynamicGroupTagPrefix.size()));
        }
    }

    void MainWindow::OnNavigationItemCollapsed(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewItemCollapsedEventArgs const& args)
    {
        auto const item = args.CollapsedItemContainer().try_as<NavigationViewItem>();
        if (item && !m_expansionStateRestoring)
        {
            StoreExpansionState(item, false);
        }
    }

    void MainWindow::OnNavigationSelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        auto const item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (!item || !m_navigation)
        {
            return;
        }

        auto const tag = ToWString(unbox_value<hstring>(item.Tag()));
        if (HasPrefix(tag, ChannelTagPrefix))
        {
            ::AstralChronicle::navigation::NavigationRequest request;
            request.Route = L"event-logs";
            request.Channel = ::AstralChronicle::models::EventChannelIdentifier{
                tag.substr(ChannelTagPrefix.size()) };
            [[maybe_unused]] auto const navigated = m_navigation->Navigate(request);
            return;
        }

        if (HasPrefix(tag, DynamicGroupTagPrefix))
        {
            auto const groupPath = tag.substr(DynamicGroupTagPrefix.size());
            auto const channel = m_dynamicChannelGroupIdentifiers.find(groupPath);
            if (channel != m_dynamicChannelGroupIdentifiers.end())
            {
                ::AstralChronicle::navigation::NavigationRequest request;
                request.Route = L"event-logs";
                request.Channel = ::AstralChronicle::models::EventChannelIdentifier{ channel->second };
                [[maybe_unused]] auto const navigated = m_navigation->Navigate(request);
            }
            return;
        }

        auto route = tag;
        if (HasPrefix(route, LandingTagPrefix))
        {
            route.erase(0, LandingTagPrefix.size());
        }

        [[maybe_unused]] auto const navigated = m_navigation->Navigate(route);
    }
}
