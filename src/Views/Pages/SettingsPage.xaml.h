#pragma once

#include "SettingsPage.g.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "DesignSystem/Theme/IThemeService.h"
#include "ViewModels/SettingsViewModel.h"

namespace winrt::AstralChronicle::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage();
        ~SettingsPage();

        [[nodiscard]] winrt::AstralChronicle::SettingsViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::design::IThemeService& theme,
            ::AstralChronicle::design::IStringResourceService const& strings);
        void OnThemeSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnLanguageSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        winrt::AstralChronicle::SettingsViewModel m_viewModel{ nullptr };
        ::AstralChronicle::design::IThemeService* m_theme{};
        std::uint32_t m_themeSubscriptionId{};
        bool m_isInitializing{};
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
