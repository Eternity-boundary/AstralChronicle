// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "MainWindow.xaml.h"
#include "App/AppHost.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/ICustomViewCatalogService.h"
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
#include "ViewModels/PersistedSettings.h"

#include "MainWindow.g.cpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstddef>
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
        using Orientation = Microsoft::UI::Xaml::Controls::Orientation;
        using ProgressRing = Microsoft::UI::Xaml::Controls::ProgressRing;
        using StackPanel = Microsoft::UI::Xaml::Controls::StackPanel;
        using TextBlock = Microsoft::UI::Xaml::Controls::TextBlock;

        constexpr std::wstring_view ChannelTagPrefix{ L"channel:" };
        constexpr std::wstring_view LandingTagPrefix{ L"landing:" };
        constexpr std::wstring_view DynamicGroupTagPrefix{ L"dynamic-group:" };
        constexpr std::wstring_view CustomViewTagPrefix{ L"custom-view:" };
        constexpr std::wstring_view CustomViewGroupTagPrefix{ L"custom-group:" };
        constexpr std::wstring_view CustomViewLoadingTag{ L"custom-view-loading" };
        constexpr std::wstring_view CustomViewsTag{ L"landing:event-logs/custom-views" };
        constexpr std::wstring_view SystemAdministrativeViewTag{ L"custom-view:system-administrative-events" };
        constexpr std::wstring_view ApplicationsAndServicesTag{ L"landing:event-logs/applications-services" };
        constexpr std::wstring_view SystemAdministrativeEventsQuery{ LR"xml(<QueryList><Query Id="0"><Select Path="Application">*[System[(Level=1 or Level=2 or Level=3)]]</Select><Select Path="Security">*[System[(Level=1 or Level=2 or Level=3)]]</Select><Select Path="System">*[System[(Level=1 or Level=2 or Level=3)]]</Select></Query></QueryList>)xml" };

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

        [[nodiscard]] hstring ShellGreetingResourceKeyForHour(std::int32_t const hour24)
        {
            // std::tm::tm_hour is always 0-23, irrespective of the user's
            // 12/24-hour display preference. In particular, 12 AM is 0 and
            // 12 PM is 12, so do not infer this value from formatted text.
            if (hour24 < 0 || hour24 >= 24)
            {
                return L"ShellGreeting.Morning";
            }

            if (hour24 >= 5 && hour24 < 12)
            {
                return L"ShellGreeting.Morning";
            }

            if (hour24 >= 12 && hour24 < 18)
            {
                return L"ShellGreeting.Afternoon";
            }

            if (hour24 >= 18 && hour24 < 22)
            {
                return L"ShellGreeting.Evening";
            }

            return L"ShellGreeting.Night";
        }

        [[nodiscard]] hstring ShellGreetingResourceKeyForCurrentLocalTime()
        {
            auto const now = std::time(nullptr);
            std::tm localTime{};
            if (localtime_s(&localTime, &now) != 0)
            {
                return L"ShellGreeting.Morning";
            }

            return ShellGreetingResourceKeyForHour(localTime.tm_hour);
        }

        [[nodiscard]] std::chrono::seconds TimeUntilNextShellGreetingBoundary()
        {
            auto const now = std::time(nullptr);
            std::tm localTime{};
            if (now == static_cast<std::time_t>(-1) || localtime_s(&localTime, &now) != 0)
            {
                return std::chrono::minutes{ 1 };
            }

            auto nextBoundary = localTime;
            nextBoundary.tm_min = 0;
            nextBoundary.tm_sec = 0;
            nextBoundary.tm_isdst = -1;

            if (localTime.tm_hour < 5)
            {
                nextBoundary.tm_hour = 5;
            }
            else if (localTime.tm_hour < 12)
            {
                nextBoundary.tm_hour = 12;
            }
            else if (localTime.tm_hour < 18)
            {
                nextBoundary.tm_hour = 18;
            }
            else if (localTime.tm_hour < 22)
            {
                nextBoundary.tm_hour = 22;
            }
            else
            {
                nextBoundary.tm_mday += 1;
                nextBoundary.tm_hour = 5;
            }

            auto const nextTime = std::mktime(&nextBoundary);
            if (nextTime == static_cast<std::time_t>(-1) || nextTime <= now)
            {
                return std::chrono::minutes{ 1 };
            }

            return std::chrono::seconds{ static_cast<std::chrono::seconds::rep>(
                std::difftime(nextTime, now)) };
        }
    }

    MainWindow::~MainWindow()
    {
        Shutdown();
    }

    void MainWindow::Shutdown() noexcept
    {
        if (m_shuttingDown)
        {
            return;
        }

        m_shuttingDown = true;
        m_shutdownRequested->store(true, std::memory_order_relaxed);

        try
        {
            if (m_greetingTimer)
            {
                m_greetingTimer.Stop();
                if (m_greetingTimerTickToken.value != 0)
                {
                    m_greetingTimer.Tick(m_greetingTimerTickToken);
                }
            }
        }
        catch (...)
        {
        }
        m_greetingTimerTickToken = {};
        m_greetingTimer = nullptr;

        try
        {
            if (m_backdropAnimationTimer)
            {
                m_backdropAnimationTimer.Stop();
                if (m_backdropAnimationTimerTickToken.value != 0)
                {
                    m_backdropAnimationTimer.Tick(m_backdropAnimationTimerTickToken);
                }
            }
        }
        catch (...)
        {
        }
        m_backdropAnimationTimerTickToken = {};
        m_backdropAnimationTimer = nullptr;

        // Releasing the navigation content first lets pages revoke service
        // subscriptions while those services are still guaranteed to exist.
        try
        {
            if (m_navigation)
            {
                m_navigation->Detach();
            }
        }
        catch (...)
        {
        }

        try
        {
            if (m_theme && m_themeSubscriptionId != 0)
            {
                m_theme->Unsubscribe(m_themeSubscriptionId);
            }
        }
        catch (...)
        {
        }
        m_themeSubscriptionId = 0;

        try
        {
            if (m_theme)
            {
                m_theme->Detach();
            }
        }
        catch (...)
        {
        }

        m_navigation.reset();
        m_theme.reset();
        m_strings.reset();
        m_eventLogCatalog.reset();
        m_customViewCatalog.reset();
    }

    void MainWindow::Initialize(::AstralChronicle::app::AppHost& host)
    {
        InitializeComponent();

        auto const weak = get_weak();
        m_closedToken = Closed([weak](auto const&, auto const&)
            {
                if (auto const self = weak.get())
                {
                    self->Shutdown();
                }
            });

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
        m_strings = host.Services().GetRequiredService<::AstralChronicle::design::IStringResourceService>();
        Title(m_strings->GetString(L"MainWindow.Title"));
        m_eventLogCatalog = host.Services().GetRequiredService<::AstralChronicle::services::IEventLogCatalogService>();
        m_customViewCatalog = host.Services().GetRequiredService<::AstralChronicle::services::ICustomViewCatalogService>();
        auto const eventQuery =
            host.Services().GetRequiredService<::AstralChronicle::services::IEventQueryService>();
        auto const eventProviders =
            host.Services().GetRequiredService<::AstralChronicle::services::IEventProviderService>();
        auto const sessions =
            host.Services().GetRequiredService<::AstralChronicle::services::ISessionRepository>();
        auto const savedViews =
            host.Services().GetRequiredService<::AstralChronicle::services::ISavedViewRepository>();
        auto const eventLive =
            host.Services().GetRequiredService<::AstralChronicle::services::IEventLiveService>();
        auto const remoteEvents =
            host.Services().GetRequiredService<::AstralChronicle::services::IRemoteEventService>();
        m_customViewQueries.insert_or_assign(
            std::wstring{ SystemAdministrativeViewTag },
            std::wstring{ SystemAdministrativeEventsQuery });
        UpdateShellGreeting();
        m_greetingTimer = RootLayout().DispatcherQueue().CreateTimer();
        m_greetingTimer.Interval(TimeUntilNextShellGreetingBoundary());
        m_greetingTimerTickToken = m_greetingTimer.Tick([weak](auto const&, auto const&)
            {
                if (auto const self = weak.get())
                {
                    self->UpdateShellGreeting();
                    self->m_greetingTimer.Interval(TimeUntilNextShellGreetingBoundary());
                }
            });
        m_greetingTimer.Start();
        m_theme = host.Services().GetRequiredService<::AstralChronicle::design::IThemeService>();
        m_theme->Initialize(RootLayout());
        m_themeSubscriptionId = m_theme->Subscribe([weak](::AstralChronicle::design::ThemeMode)
            {
                if (auto const self = weak.get())
                {
                    self->ApplyThemeBackdrop();
                }
            });
        ApplyThemeBackdrop();

        m_navigation = host.Services().GetRequiredService<::AstralChronicle::navigation::INavigationService>();
        m_navigation->Attach(ContentFrame());
        m_navigation->Register({
            L"dashboard",
            [weak, eventQuery, strings = m_strings]()
            {
                auto const self = weak.get();
                if (!self)
                {
                    return FrameworkElement{ nullptr };
                }
                auto page = make<DashboardPage>();
                get_self<DashboardPage>(page)->Initialize(
                    eventQuery,
                    strings,
                    self->RootLayout().DispatcherQueue(),
                    *self->m_navigation,
                    [weak](std::wstring_view route)
                    {
                        if (auto const window = weak.get())
                        {
                            window->SelectNavigationItemForRoute(route);
                        }
                    });
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"event-logs",
            [eventQuery, strings = m_strings]()
            {
                auto page = make<EventLogsPage>();
                get_self<EventLogsPage>(page)->Initialize(eventQuery, strings);
                return page.as<FrameworkElement>();
            },
            [eventQuery, strings = m_strings](
                ::AstralChronicle::navigation::NavigationRequest const& request)
            {
                auto page = make<EventLogsPage>();
                get_self<EventLogsPage>(page)->Initialize(
                    eventQuery,
                    strings,
                    request.Channel,
                    request.Query);
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"timeline",
            [weak, eventQuery, strings = m_strings]()
            {
                auto const self = weak.get();
                if (!self)
                {
                    return FrameworkElement{ nullptr };
                }
                auto page = make<TimelinePage>();
                get_self<TimelinePage>(page)->Initialize(
                    eventQuery,
                    strings,
                    self->RootLayout().DispatcherQueue(),
                    *self->m_navigation,
                    [weak](std::wstring_view route)
                    {
                        if (auto const window = weak.get())
                        {
                            window->SelectNavigationItemForRoute(route);
                        }
                    });
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"providers",
            [weak, eventProviders, strings = m_strings]()
            {
                auto const self = weak.get();
                if (!self)
                {
                    return FrameworkElement{ nullptr };
                }
                auto page = make<ProvidersPage>();
                get_self<ProvidersPage>(page)->Initialize(
                    eventProviders,
                    strings,
                    *self->m_navigation,
                    [weak](std::wstring_view route)
                    {
                        if (auto const window = weak.get())
                        {
                            window->SelectNavigationItemForRoute(route);
                        }
                    });
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"sessions",
            [sessions, strings = m_strings]()
            {
                auto page = make<SessionsPage>();
                get_self<SessionsPage>(page)->Initialize(sessions, strings);
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"saved-views",
            [weak, savedViews, strings = m_strings]()
            {
                auto const self = weak.get();
                if (!self)
                {
                    return FrameworkElement{ nullptr };
                }
                auto page = make<SavedViewsPage>();
                get_self<SavedViewsPage>(page)->Initialize(
                    savedViews,
                    strings,
                    *self->m_navigation,
                    [weak](std::wstring_view route)
                    {
                        if (auto const window = weak.get())
                        {
                            window->SelectNavigationItemForRoute(route);
                        }
                    });
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"live",
            [eventLive, strings = m_strings]()
            {
                auto page = make<LivePage>();
                get_self<LivePage>(page)->Initialize(eventLive, strings);
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"remote",
            [remoteEvents, eventQuery, strings = m_strings]()
            {
                auto page = make<RemotePage>();
                get_self<RemotePage>(page)->Initialize(remoteEvents, eventQuery, strings);
                return page.as<FrameworkElement>();
            } });
        m_navigation->Register({
            L"settings",
            [theme = m_theme, strings = m_strings]
            {
                auto page = make<SettingsPage>();
                get_self<SettingsPage>(page)->Initialize(theme, strings);
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
        SetThemeBackdropLayout(NavigationPaneWidth());
    }

    void MainWindow::SetThemeBackdropLayout(double const paneWidth)
    {
        auto const contentMargin = Thickness{ paneWidth, 0.0, 0.0, 0.0 };

        ThemeNavigationBackground().Width(paneWidth);
        ThemeNavigationContrastOverlay().Width(paneWidth);
        ThemeMainBackground().Margin(contentMargin);
        ThemeMainContrastOverlay().Margin(contentMargin);
    }

    void MainWindow::AnimateThemeBackdropLayout(double const targetWidth)
    {
        auto const animationsEnabled =
            ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load()
                .AnimationsEnabled;
        if (!animationsEnabled)
        {
            if (m_backdropAnimationTimer)
            {
                m_backdropAnimationTimer.Stop();
            }
            SetThemeBackdropLayout(targetWidth);
            return;
        }

        if (!m_backdropAnimationTimer)
        {
            m_backdropAnimationTimer = RootLayout().DispatcherQueue().CreateTimer();
            m_backdropAnimationTimer.Interval(std::chrono::milliseconds{ 16 });
            auto const weak = get_weak();
            m_backdropAnimationTimerTickToken = m_backdropAnimationTimer.Tick([weak](auto const&, auto const&)
            {
                auto const self = weak.get();
                if (!self)
                {
                    return;
                }
                constexpr double durationMilliseconds = 260.0;
                auto const elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - self->m_backdropAnimationStart).count();
                auto const progress = std::min(1.0, elapsed / durationMilliseconds);
                auto const easedProgress = progress * (2.0 - progress);
                self->SetThemeBackdropLayout(
                    self->m_backdropAnimationFromWidth
                    + (self->m_backdropAnimationTargetWidth - self->m_backdropAnimationFromWidth) * easedProgress);
                if (progress >= 1.0)
                {
                    self->m_backdropAnimationTimer.Stop();
                    self->SetThemeBackdropLayout(self->m_backdropAnimationTargetWidth);
                }
            });
        }

        m_backdropAnimationFromWidth = ThemeNavigationBackground().Width();
        m_backdropAnimationTargetWidth = targetWidth;
        m_backdropAnimationStart = std::chrono::steady_clock::now();
        if (std::abs(m_backdropAnimationTargetWidth - m_backdropAnimationFromWidth) < 0.1)
        {
            m_backdropAnimationTimer.Stop();
            SetThemeBackdropLayout(m_backdropAnimationTargetWidth);
            return;
        }
        m_backdropAnimationTimer.Start();
    }

    double MainWindow::NavigationPaneWidth()
    {
        auto const navigationView = RootNavigationView();
        if (navigationView.PaneDisplayMode() == Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::Left)
        {
            return navigationView.IsPaneOpen()
                ? navigationView.OpenPaneLength()
                : navigationView.CompactPaneLength();
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
        UpdateThemeBackdropLayout();
    }

    void MainWindow::OnNavigationPaneOpened(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Windows::Foundation::IInspectable const&)
    {
        if (m_backdropAnimationTimer)
        {
            m_backdropAnimationTimer.Stop();
        }
        UpdateThemeBackdropLayout();
    }

    void MainWindow::OnNavigationPaneOpening(
        Microsoft::UI::Xaml::Controls::NavigationView const& sender,
        Windows::Foundation::IInspectable const&)
    {
        AnimateThemeBackdropLayout(sender.OpenPaneLength());
    }

    void MainWindow::OnNavigationPaneClosed(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Windows::Foundation::IInspectable const&)
    {
        if (m_backdropAnimationTimer)
        {
            m_backdropAnimationTimer.Stop();
        }
        UpdateThemeBackdropLayout();
    }

    void MainWindow::OnNavigationPaneClosing(
        Microsoft::UI::Xaml::Controls::NavigationView const& sender,
        Microsoft::UI::Xaml::Controls::NavigationViewPaneClosingEventArgs const&)
    {
        AnimateThemeBackdropLayout(sender.CompactPaneLength());
    }

    void MainWindow::SelectNavigationItemForRoute(std::wstring_view route)
    {
        auto findItem = [route](auto const& items) -> NavigationViewItem
        {
            for (auto const& value : items)
            {
                auto const item = value.try_as<NavigationViewItem>();
                if (item && TagOf(item) == route)
                {
                    return item;
                }
            }

            return nullptr;
        };

        auto item = findItem(RootNavigationView().MenuItems());
        if (!item)
        {
            item = findItem(RootNavigationView().FooterMenuItems());
        }

        if (!item || RootNavigationView().SelectedItem() == item)
        {
            return;
        }

        m_isUpdatingNavigationSelection = true;
        RootNavigationView().SelectedItem(item);
        m_isUpdatingNavigationSelection = false;
    }

    void MainWindow::StartDynamicChannelLoad()
    {
        if (m_dynamicChannelLoadRequested || m_dynamicChannelTreeLoaded || !m_eventLogCatalog)
        {
            return;
        }

        m_dynamicChannelLoadRequested = true;
        if (m_dynamicChannelRoot)
        {
            ShowDynamicChannelLoadingState(m_dynamicChannelRoot);
        }
        LoadDynamicChannelsAsync();
    }

    void MainWindow::StartCustomViewLoad()
    {
        if (m_customViewLoadRequested || m_customViewTreeLoaded || !m_customViewCatalog)
        {
            return;
        }

        m_customViewLoadRequested = true;
        if (m_customViewRoot)
        {
            NavigationViewItem loading;
            loading.Content(box_value(m_strings->GetString(L"EventLogs.CustomViewsLoading.Text")));
            loading.Tag(box_value(hstring{ CustomViewLoadingTag }));
            loading.IsEnabled(false);
            loading.SelectsOnInvoked(false);
            m_customViewRoot.MenuItems().Append(loading);
        }
        LoadCustomViewsAsync();
    }

    void MainWindow::RemoveCustomViewLoadingState()
    {
        if (!m_customViewRoot)
        {
            return;
        }

        auto const items = m_customViewRoot.MenuItems();
        for (std::uint32_t index = items.Size(); index-- > 0;)
        {
            auto const item = items.GetAt(index).try_as<NavigationViewItem>();
            if (item && TagOf(item) == CustomViewLoadingTag)
            {
                items.RemoveAt(index);
            }
        }
    }

    winrt::fire_and_forget MainWindow::LoadCustomViewsAsync()
    {
        auto lifetime = get_strong();
        try
        {
            auto const cancellation = m_shutdownRequested;
            auto const catalog = m_customViewCatalog;
            auto const strings = m_strings;
            if (!catalog || !strings || cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            auto const dispatcher = RootLayout().DispatcherQueue();
            std::vector<::AstralChronicle::models::CustomViewTreeNode> tree;
            bool failed{};

            co_await winrt::resume_background();
            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            try
            {
                tree = catalog->Enumerate();
            }
            catch (...)
            {
                failed = true;
            }

            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            co_await wil::resume_foreground(dispatcher);
            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            RemoveCustomViewLoadingState();
            if (failed)
            {
                m_customViewLoadRequested = false;
                if (m_customViewRoot)
                {
                    NavigationViewItem unavailable;
                    unavailable.Content(box_value(strings->GetString(L"EventLogs.CustomViewsUnavailable.Text")));
                    unavailable.IsEnabled(false);
                    m_customViewRoot.MenuItems().Append(unavailable);
                }
                co_return;
            }

            m_customViewQueries.clear();
            m_customViewQueries.insert_or_assign(
                std::wstring{ SystemAdministrativeViewTag },
                std::wstring{ SystemAdministrativeEventsQuery });
            m_customViewTree = std::move(tree);
            m_customViewTreeLoaded = true;
            if (!m_customViewRoot)
            {
                co_return;
            }

            for (auto const& node : *m_customViewTree)
            {
                m_customViewRoot.MenuItems().Append(CreateCustomViewNavigationItem(node));
            }
            m_customViewRoot.HasUnrealizedChildren(false);
        }
        catch (...)
        {
            // App shutdown and dispatcher teardown are normal cancellation paths.
        }
    }

    winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem MainWindow::CreateCustomViewNavigationItem(
        ::AstralChronicle::models::CustomViewTreeNode const& node)
    {
        NavigationViewItem item;
        item.Content(box_value(hstring{ node.Segment }));

        if (node.IsView)
        {
            auto const tag = std::wstring{ CustomViewTagPrefix } + node.RelativePath;
            item.Tag(box_value(hstring{ tag }));
            m_customViewQueries.insert_or_assign(tag, node.Query);
            return item;
        }

        auto const tag = std::wstring{ CustomViewGroupTagPrefix } + node.RelativePath;
        item.Tag(box_value(hstring{ tag }));
        item.SelectsOnInvoked(false);
        for (auto const& child : node.Children)
        {
            item.MenuItems().Append(CreateCustomViewNavigationItem(child));
        }
        item.HasUnrealizedChildren(false);
        return item;
    }

    void MainWindow::ShowDynamicChannelLoadingState(NavigationViewItem const& parent)
    {
        parent.MenuItems().Clear();

        StackPanel loadingContent;
        loadingContent.Orientation(Orientation::Horizontal);
        loadingContent.Spacing(8.0);

        ProgressRing progress;
        progress.IsActive(true);
        progress.Width(16.0);
        progress.Height(16.0);
        progress.IsHitTestVisible(false);
        loadingContent.Children().Append(progress);

        TextBlock loadingText;
        loadingText.Text(m_strings
            ? m_strings->GetString(L"EventLogs.DynamicChannelsLoading.Text")
            : hstring{ L"Loading…" });
        loadingContent.Children().Append(loadingText);

        NavigationViewItem loadingItem;
        loadingItem.Content(loadingContent);
        loadingItem.IsEnabled(false);
        loadingItem.SelectsOnInvoked(false);
        parent.MenuItems().Append(loadingItem);
        parent.HasUnrealizedChildren(false);
    }

    winrt::fire_and_forget MainWindow::LoadDynamicChannelsAsync()
    {
        auto lifetime = get_strong();
        try
        {
            auto const cancellation = m_shutdownRequested;
            auto const catalog = m_eventLogCatalog;
            auto const strings = m_strings;
            if (!catalog || !strings || cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            auto const dispatcher = RootLayout().DispatcherQueue();
            std::optional<::AstralChronicle::services::ChannelPathTreeNode> tree;
            bool failed{};

            co_await winrt::resume_background();
            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            try
            {
                auto const channels = catalog->EnumerateChannels();
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

            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            co_await wil::resume_foreground(dispatcher);
            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }

            if (failed || !tree)
            {
                m_dynamicChannelLoadRequested = false;
                if (m_dynamicChannelRoot)
                {
                    m_dynamicChannelRoot.MenuItems().Clear();
                    NavigationViewItem unavailable;
                    unavailable.Content(box_value(strings->GetString(L"EventLogs.DynamicChannelsUnavailable.Text")));
                    unavailable.IsEnabled(false);
                    m_dynamicChannelRoot.MenuItems().Append(unavailable);
                    m_dynamicChannelRoot.HasUnrealizedChildren(false);
                }
                co_return;
            }

            m_dynamicChannelTree = std::move(tree);
            m_dynamicChannelIndex.clear();
            m_dynamicChannelGroupIdentifiers.clear();
            m_populatingDynamicPaths.clear();
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
                m_dynamicChannelRoot.MenuItems().Clear();
                NavigationViewItem noChannels;
                noChannels.Content(box_value(strings->GetString(L"EventLogs.DynamicChannelsNone.Text")));
                noChannels.IsEnabled(false);
                m_dynamicChannelRoot.MenuItems().Append(noChannels);
                m_dynamicChannelRoot.HasUnrealizedChildren(false);
                co_return;
            }

            StartDynamicChildrenPopulation(m_dynamicChannelRoot, {});
        }
        catch (...)
        {
            // App shutdown and dispatcher teardown are normal cancellation paths.
        }
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

    void MainWindow::StartDynamicChildrenPopulation(
        NavigationViewItem const& parent,
        std::wstring_view parentPath)
    {
        auto const key = std::wstring{ parentPath };
        if (m_populatedDynamicPaths.find(key) != m_populatedDynamicPaths.end()
            || m_populatingDynamicPaths.find(key) != m_populatingDynamicPaths.end())
        {
            return;
        }

        ShowDynamicChannelLoadingState(parent);
        PopulateDynamicChildrenAsync(parent, key);
    }

    NavigationViewItem MainWindow::CreateDynamicNavigationItem(
        ::AstralChronicle::services::ChannelPathTreeNode const& node)
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
        return item;
    }

    winrt::fire_and_forget MainWindow::PopulateDynamicChildrenAsync(
        NavigationViewItem const& parent,
        std::wstring parentPath)
    {
        auto lifetime = get_strong();
        try
        {
            auto const cancellation = m_shutdownRequested;
            if (cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            auto const dispatcher = RootLayout().DispatcherQueue();
            auto const key = parentPath;

            if (m_populatedDynamicPaths.find(key) != m_populatedDynamicPaths.end()
                || m_populatingDynamicPaths.find(key) != m_populatingDynamicPaths.end())
            {
                co_return;
            }

            std::vector<::AstralChronicle::services::ChannelPathTreeNode> const* children{};
            if (key.empty())
            {
                if (!m_dynamicChannelTree)
                {
                    co_return;
                }
                children = &m_dynamicChannelTree->Children;
            }
            else
            {
                auto const node = m_dynamicChannelIndex.find(key);
                if (node == m_dynamicChannelIndex.end())
                {
                    co_return;
                }
                children = &node->second->Children;
            }

            m_populatingDynamicPaths.insert(key);
            parent.MenuItems().Clear();

            constexpr std::size_t ItemsPerBatch = 32;
            for (std::size_t index = 0; index < children->size(); ++index)
            {
                auto const& node = (*children)[index];
                auto item = CreateDynamicNavigationItem(node);
                parent.MenuItems().Append(item);

                if ((index + 1) % ItemsPerBatch == 0 && index + 1 < children->size())
                {
                    co_await winrt::resume_background();
                    if (cancellation->load(std::memory_order_relaxed))
                    {
                        co_return;
                    }
                    co_await wil::resume_foreground(dispatcher);
                    if (cancellation->load(std::memory_order_relaxed))
                    {
                        co_return;
                    }
                }
            }

            parent.HasUnrealizedChildren(false);
            m_populatingDynamicPaths.erase(key);
            m_populatedDynamicPaths.insert(key);
        }
        catch (...)
        {
            // App shutdown and dispatcher teardown are normal cancellation paths.
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

        auto const tag = TagOf(item);
        if (tag == CustomViewsTag)
        {
            m_customViewRoot = item;
            StartCustomViewLoad();
            return;
        }

        if (tag == ApplicationsAndServicesTag)
        {
            m_dynamicChannelRoot = item;
            StartDynamicChannelLoad();
            return;
        }

        if (HasPrefix(tag, DynamicGroupTagPrefix))
        {
            StartDynamicChildrenPopulation(item, std::wstring_view{ tag }.substr(DynamicGroupTagPrefix.size()));
        }
    }

    void MainWindow::OnNavigationItemCollapsed(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewItemCollapsedEventArgs const& args)
    {
        (void)args;
    }

    void MainWindow::OnNavigationSelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        if (m_isUpdatingNavigationSelection)
        {
            return;
        }

        auto const item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (!item || !m_navigation)
        {
            return;
        }

        auto const tag = ToWString(unbox_value<hstring>(item.Tag()));
        if (HasPrefix(tag, CustomViewTagPrefix))
        {
            auto const customView = m_customViewQueries.find(tag);
            if (customView != m_customViewQueries.end() && !customView->second.empty())
            {
                ::AstralChronicle::navigation::NavigationRequest request;
                request.Route = L"event-logs";
                request.Channel = ::AstralChronicle::models::EventChannelIdentifier{};
                request.Query = customView->second;
                [[maybe_unused]] auto const navigated = m_navigation->Navigate(request);
            }
            return;
        }

        if (HasPrefix(tag, CustomViewGroupTagPrefix))
        {
            return;
        }

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
