// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "MainWindow.g.h"

#include "DesignSystem/Theme/IThemeService.h"

namespace AstralChronicle::app
{
    class AppHost;
}

namespace AstralChronicle::navigation
{
    struct INavigationService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;
        ~MainWindow();

        void Initialize(::AstralChronicle::app::AppHost& host);
        void OnNavigationSelectionChanged(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
        void OnNavigationViewLoaded(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnNavigationPaneOpened(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void OnNavigationPaneClosed(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

    private:
        void UpdateShellGreeting();
        void ApplyThemeBackdrop();
        void UpdateThemeBackdropLayout();
        double NavigationPaneWidth();

        ::AstralChronicle::navigation::INavigationService* m_navigation{};
        ::AstralChronicle::design::IStringResourceService* m_strings{};
        ::AstralChronicle::design::IThemeService* m_theme{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_greetingTimer{ nullptr };
        std::uint32_t m_themeSubscriptionId{};
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
