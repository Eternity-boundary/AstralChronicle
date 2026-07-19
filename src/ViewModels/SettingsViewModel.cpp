// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SettingsViewModel.h"

#include "SettingsViewModel.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    void SettingsViewModel::Initialize(
        std::int32_t selectedThemeIndex,
        winrt::hstring const& heading,
        winrt::hstring const& description,
        winrt::hstring const& themeHint)
    {
        m_heading = heading;
        m_description = description;
        m_themeHint = themeHint;
        SelectedThemeIndex(selectedThemeIndex);
        RaisePropertyChanged(L"Heading");
        RaisePropertyChanged(L"Description");
        RaisePropertyChanged(L"ThemeHint");
    }

    winrt::hstring SettingsViewModel::Heading() const
    {
        return m_heading;
    }

    winrt::hstring SettingsViewModel::Description() const
    {
        return m_description;
    }

    winrt::hstring SettingsViewModel::ThemeHint() const
    {
        return m_themeHint;
    }

    std::int32_t SettingsViewModel::SelectedThemeIndex() const noexcept
    {
        return m_selectedThemeIndex;
    }

    void SettingsViewModel::SelectedThemeIndex(std::int32_t value)
    {
        if (m_selectedThemeIndex == value)
        {
            return;
        }

        m_selectedThemeIndex = value;
        RaisePropertyChanged(L"SelectedThemeIndex");
    }

    winrt::event_token SettingsViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void SettingsViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void SettingsViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
