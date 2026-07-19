// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "SettingsViewModel.g.h"

namespace winrt::AstralChronicle::implementation
{
    struct SettingsViewModel : SettingsViewModelT<SettingsViewModel>
    {
        SettingsViewModel() = default;

        void Initialize(
            std::int32_t selectedThemeIndex,
            winrt::hstring const& heading,
            winrt::hstring const& description,
            winrt::hstring const& themeHint);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Description() const;
        [[nodiscard]] winrt::hstring ThemeHint() const;
        [[nodiscard]] std::int32_t SelectedThemeIndex() const noexcept;
        void SelectedThemeIndex(std::int32_t value);

        winrt::event_token PropertyChanged(
            Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(winrt::hstring const& propertyName);

        winrt::hstring m_heading;
        winrt::hstring m_description;
        winrt::hstring m_themeHint;
        std::int32_t m_selectedThemeIndex{};
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SettingsViewModel : SettingsViewModelT<SettingsViewModel, implementation::SettingsViewModel>
    {
    };
}
