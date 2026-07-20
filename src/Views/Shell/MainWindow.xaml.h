// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "MainWindow.g.h"

#include "Models/EventChannelDescriptor.h"
#include "Models/CustomViewTreeNode.h"
#include "Services/ChannelPathGrouping.h"
#include "DesignSystem/Theme/IThemeService.h"

#include <winrt/Windows.Foundation.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AstralChronicle::app
{
    class AppHost;
}

namespace AstralChronicle::navigation
{
    struct INavigationService;
}

namespace AstralChronicle::services
{
    struct IEventLogCatalogService;
    struct ICustomViewCatalogService;
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
        void OnNavigationItemExpanding(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            Microsoft::UI::Xaml::Controls::NavigationViewItemExpandingEventArgs const& args);
        void OnNavigationItemCollapsed(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            Microsoft::UI::Xaml::Controls::NavigationViewItemCollapsedEventArgs const& args);
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
        void SelectNavigationItemForRoute(std::wstring_view route);
        void UpdateShellGreeting();
        void ApplyThemeBackdrop();
        void UpdateThemeBackdropLayout();
        double NavigationPaneWidth();
        void StartDynamicChannelLoad();
        winrt::fire_and_forget LoadDynamicChannelsAsync();
        void StartCustomViewLoad();
        winrt::fire_and_forget LoadCustomViewsAsync();
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem CreateCustomViewNavigationItem(
            ::AstralChronicle::models::CustomViewTreeNode const& node);
        void RemoveCustomViewLoadingState();
        void StartDynamicChildrenPopulation(
            Microsoft::UI::Xaml::Controls::NavigationViewItem const& parent,
            std::wstring_view parentPath);
        void ShowDynamicChannelLoadingState(
            Microsoft::UI::Xaml::Controls::NavigationViewItem const& parent);
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem CreateDynamicNavigationItem(
            ::AstralChronicle::services::ChannelPathTreeNode const& node);
        winrt::fire_and_forget PopulateDynamicChildrenAsync(
            Microsoft::UI::Xaml::Controls::NavigationViewItem const& parent,
            std::wstring parentPath);
        void IndexDynamicChannelNodes(::AstralChronicle::services::ChannelPathTreeNode& node);

        ::AstralChronicle::navigation::INavigationService* m_navigation{};
        ::AstralChronicle::design::IStringResourceService* m_strings{};
        ::AstralChronicle::design::IThemeService* m_theme{};
        ::AstralChronicle::services::IEventLogCatalogService* m_eventLogCatalog{};
        ::AstralChronicle::services::ICustomViewCatalogService* m_customViewCatalog{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_greetingTimer{ nullptr };
        std::uint32_t m_themeSubscriptionId{};
        bool m_dynamicChannelLoadRequested{};
        bool m_dynamicChannelTreeLoaded{};
        bool m_customViewLoadRequested{};
        bool m_customViewTreeLoaded{};
        bool m_isUpdatingNavigationSelection{};
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem m_dynamicChannelRoot{ nullptr };
        std::optional<::AstralChronicle::services::ChannelPathTreeNode> m_dynamicChannelTree;
        std::unordered_map<std::wstring, ::AstralChronicle::services::ChannelPathTreeNode*> m_dynamicChannelIndex;
        std::unordered_map<std::wstring, std::wstring> m_dynamicChannelGroupIdentifiers;
        std::unordered_set<std::wstring> m_populatingDynamicPaths;
        std::unordered_set<std::wstring> m_populatedDynamicPaths;
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem m_customViewRoot{ nullptr };
        std::optional<std::vector<::AstralChronicle::models::CustomViewTreeNode>> m_customViewTree;
        std::unordered_map<std::wstring, std::wstring> m_customViewQueries;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
