// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "MainWindow.xaml.h"
#include "App/AppHost.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Views/Pages/DashboardPage.xaml.h"
#include "Views/Pages/FeaturePlaceholderPage.xaml.h"
#include "Views/Pages/SettingsPage.xaml.h"

#include "MainWindow.g.cpp"

#include <chrono>
#include <ctime>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::AstralChronicle::implementation
{
    namespace
    {
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

        SystemBackdrop(Microsoft::UI::Xaml::Media::MicaBackdrop{});
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        Title(host.Strings().GetString(L"MainWindow.Title"));
        m_strings = &host.Strings();
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
            [&host]()
            {
                auto page = make<DashboardPage>();
                get_self<DashboardPage>(page)->Initialize(
                    host.EventLogCatalog(), host.EventQuery(), host.Strings());
                return page.as<FrameworkElement>();
            } });
        auto registerPlaceholder = [this, &host](
            hstring const& route,
            hstring const& headingResourceKey,
            hstring const& descriptionResourceKey)
            {
                m_navigation->Register(::AstralChronicle::navigation::PageRegistration{
                    std::wstring{ route.c_str(), route.size() },
                    [&host, headingResourceKey, descriptionResourceKey]
                    {
                        auto page = make<FeaturePlaceholderPage>();
                        get_self<FeaturePlaceholderPage>(page)->Initialize(
                            host.Strings().GetString(headingResourceKey),
                            host.Strings().GetString(descriptionResourceKey));
                        return page.as<FrameworkElement>();
                    } });
            };

        registerPlaceholder(
            L"event-logs",
            L"Feature.EventLogs.Heading",
            L"Feature.EventLogs.Description");
        registerPlaceholder(
            L"timeline",
            L"Feature.Timeline.Heading",
            L"Feature.Timeline.Description");
        registerPlaceholder(
            L"providers",
            L"Feature.Providers.Heading",
            L"Feature.Providers.Description");
        registerPlaceholder(
            L"sessions",
            L"Feature.Sessions.Heading",
            L"Feature.Sessions.Description");
        registerPlaceholder(
            L"saved-views",
            L"Feature.SavedViews.Heading",
            L"Feature.SavedViews.Description");
        registerPlaceholder(
            L"live",
            L"Feature.Live.Heading",
            L"Feature.Live.Description");
        registerPlaceholder(
            L"remote",
            L"Feature.Remote.Heading",
            L"Feature.Remote.Description");
        m_navigation->Register({
            L"settings",
            [&host]
            {
                auto page = make<SettingsPage>();
                get_self<SettingsPage>(page)->Initialize(host.Theme(), host.Strings());
                return page.as<FrameworkElement>();
            } });

        RootNavigationView().SelectedItem(RootNavigationView().MenuItems().GetAt(0));
        [[maybe_unused]] auto const openedDashboard = m_navigation->Navigate(L"dashboard");
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

    void MainWindow::OnNavigationSelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        auto const item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (!item || !m_navigation)
        {
            return;
        }

        auto const route = unbox_value<hstring>(item.Tag());
        [[maybe_unused]] auto const navigated = m_navigation->Navigate(route.c_str());
    }
}
