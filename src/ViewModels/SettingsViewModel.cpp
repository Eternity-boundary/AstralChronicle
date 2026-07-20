// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SettingsViewModel.h"

#include "SettingsViewModel.g.cpp"

#include <winrt/Windows.Storage.h>

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
        LoadPersistedSettings();
        SelectedThemeIndex(selectedThemeIndex);
        RaisePropertyChanged(L"Heading");
        RaisePropertyChanged(L"Description");
        RaisePropertyChanged(L"ThemeHint");
        RaisePropertyChanged(L"DefaultSortDescending");
        RaisePropertyChanged(L"UseUtc");
        RaisePropertyChanged(L"GroupRepeatedEvents");
        RaisePropertyChanged(L"DetailsPaneOpen");
        RaisePropertyChanged(L"AnimationsEnabled");
        RaisePropertyChanged(L"LiveQueueLimit");
        RaisePropertyChanged(L"QueryBatchSize");
        RaisePropertyChanged(L"MaxVisibleLiveRows");
        RaisePropertyChanged(L"PersistSessions");
        RaisePropertyChanged(L"PersistBookmarks");
        RaisePropertyChanged(L"CacheProviderMetadata");
        RaisePropertyChanged(L"RedactComputerName");
        RaisePropertyChanged(L"RedactUserNames");
        RaisePropertyChanged(L"RawXPathMode");
        RaisePropertyChanged(L"DebugLogging");
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

    bool SettingsViewModel::DefaultSortDescending() const noexcept { return m_defaultSortDescending; }
    void SettingsViewModel::DefaultSortDescending(bool value)
    {
        if (m_defaultSortDescending == value) return;
        m_defaultSortDescending = value;
        SaveBoolSetting(L"EventDisplay.DefaultSortDescending", value);
        RaisePropertyChanged(L"DefaultSortDescending");
    }

    bool SettingsViewModel::UseUtc() const noexcept { return m_useUtc; }
    void SettingsViewModel::UseUtc(bool value)
    {
        if (m_useUtc == value) return;
        m_useUtc = value;
        SaveBoolSetting(L"EventDisplay.UseUtc", value);
        RaisePropertyChanged(L"UseUtc");
    }

    bool SettingsViewModel::GroupRepeatedEvents() const noexcept { return m_groupRepeatedEvents; }
    void SettingsViewModel::GroupRepeatedEvents(bool value)
    {
        if (m_groupRepeatedEvents == value) return;
        m_groupRepeatedEvents = value;
        SaveBoolSetting(L"EventDisplay.GroupRepeatedEvents", value);
        RaisePropertyChanged(L"GroupRepeatedEvents");
    }

    bool SettingsViewModel::DetailsPaneOpen() const noexcept { return m_detailsPaneOpen; }
    void SettingsViewModel::DetailsPaneOpen(bool value)
    {
        if (m_detailsPaneOpen == value) return;
        m_detailsPaneOpen = value;
        SaveBoolSetting(L"EventDisplay.DetailsPaneOpen", value);
        RaisePropertyChanged(L"DetailsPaneOpen");
    }

    bool SettingsViewModel::AnimationsEnabled() const noexcept { return m_animationsEnabled; }
    void SettingsViewModel::AnimationsEnabled(bool value)
    {
        if (m_animationsEnabled == value) return;
        m_animationsEnabled = value;
        SaveBoolSetting(L"Appearance.AnimationsEnabled", value);
        RaisePropertyChanged(L"AnimationsEnabled");
    }

    winrt::hstring SettingsViewModel::LiveQueueLimit() const { return m_liveQueueLimit; }
    void SettingsViewModel::LiveQueueLimit(winrt::hstring const& value)
    {
        if (m_liveQueueLimit == value) return;
        m_liveQueueLimit = value;
        SaveTextSetting(L"Monitoring.LiveQueueLimit", value);
        RaisePropertyChanged(L"LiveQueueLimit");
    }

    winrt::hstring SettingsViewModel::QueryBatchSize() const { return m_queryBatchSize; }
    void SettingsViewModel::QueryBatchSize(winrt::hstring const& value)
    {
        if (m_queryBatchSize == value) return;
        m_queryBatchSize = value;
        SaveTextSetting(L"Performance.QueryBatchSize", value);
        RaisePropertyChanged(L"QueryBatchSize");
    }

    winrt::hstring SettingsViewModel::MaxVisibleLiveRows() const { return m_maxVisibleLiveRows; }
    void SettingsViewModel::MaxVisibleLiveRows(winrt::hstring const& value)
    {
        if (m_maxVisibleLiveRows == value) return;
        m_maxVisibleLiveRows = value;
        SaveTextSetting(L"Performance.MaxVisibleLiveRows", value);
        RaisePropertyChanged(L"MaxVisibleLiveRows");
    }

    bool SettingsViewModel::PersistSessions() const noexcept { return m_persistSessions; }
    void SettingsViewModel::PersistSessions(bool value)
    {
        if (m_persistSessions == value) return;
        m_persistSessions = value;
        SaveBoolSetting(L"Storage.PersistSessions", value);
        RaisePropertyChanged(L"PersistSessions");
    }

    bool SettingsViewModel::PersistBookmarks() const noexcept { return m_persistBookmarks; }
    void SettingsViewModel::PersistBookmarks(bool value)
    {
        if (m_persistBookmarks == value) return;
        m_persistBookmarks = value;
        SaveBoolSetting(L"Storage.PersistBookmarks", value);
        RaisePropertyChanged(L"PersistBookmarks");
    }

    bool SettingsViewModel::CacheProviderMetadata() const noexcept { return m_cacheProviderMetadata; }
    void SettingsViewModel::CacheProviderMetadata(bool value)
    {
        if (m_cacheProviderMetadata == value) return;
        m_cacheProviderMetadata = value;
        SaveBoolSetting(L"Storage.CacheProviderMetadata", value);
        RaisePropertyChanged(L"CacheProviderMetadata");
    }

    bool SettingsViewModel::RedactComputerName() const noexcept { return m_redactComputerName; }
    void SettingsViewModel::RedactComputerName(bool value)
    {
        if (m_redactComputerName == value) return;
        m_redactComputerName = value;
        SaveBoolSetting(L"Privacy.RedactComputerName", value);
        RaisePropertyChanged(L"RedactComputerName");
    }

    bool SettingsViewModel::RedactUserNames() const noexcept { return m_redactUserNames; }
    void SettingsViewModel::RedactUserNames(bool value)
    {
        if (m_redactUserNames == value) return;
        m_redactUserNames = value;
        SaveBoolSetting(L"Privacy.RedactUserNames", value);
        RaisePropertyChanged(L"RedactUserNames");
    }

    bool SettingsViewModel::RawXPathMode() const noexcept { return m_rawXPathMode; }
    void SettingsViewModel::RawXPathMode(bool value)
    {
        if (m_rawXPathMode == value) return;
        m_rawXPathMode = value;
        SaveBoolSetting(L"Advanced.RawXPathMode", value);
        RaisePropertyChanged(L"RawXPathMode");
    }

    bool SettingsViewModel::DebugLogging() const noexcept { return m_debugLogging; }
    void SettingsViewModel::DebugLogging(bool value)
    {
        if (m_debugLogging == value) return;
        m_debugLogging = value;
        SaveBoolSetting(L"Advanced.DebugLogging", value);
        RaisePropertyChanged(L"DebugLogging");
    }

    void SettingsViewModel::LoadPersistedSettings()
    {
        try
        {
            auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto readBool = [&values](wchar_t const* key, bool fallback)
            {
                try { return values.HasKey(key) ? winrt::unbox_value<bool>(values.Lookup(key)) : fallback; }
                catch (...) { return fallback; }
            };
            auto readText = [&values](wchar_t const* key, winrt::hstring fallback)
            {
                try { return values.HasKey(key) ? winrt::unbox_value<winrt::hstring>(values.Lookup(key)) : fallback; }
                catch (...) { return fallback; }
            };

            m_defaultSortDescending = readBool(L"EventDisplay.DefaultSortDescending", m_defaultSortDescending);
            m_useUtc = readBool(L"EventDisplay.UseUtc", m_useUtc);
            m_groupRepeatedEvents = readBool(L"EventDisplay.GroupRepeatedEvents", m_groupRepeatedEvents);
            m_detailsPaneOpen = readBool(L"EventDisplay.DetailsPaneOpen", m_detailsPaneOpen);
            m_animationsEnabled = readBool(L"Appearance.AnimationsEnabled", m_animationsEnabled);
            m_liveQueueLimit = readText(L"Monitoring.LiveQueueLimit", m_liveQueueLimit);
            m_queryBatchSize = readText(L"Performance.QueryBatchSize", m_queryBatchSize);
            m_maxVisibleLiveRows = readText(L"Performance.MaxVisibleLiveRows", m_maxVisibleLiveRows);
            m_persistSessions = readBool(L"Storage.PersistSessions", m_persistSessions);
            m_persistBookmarks = readBool(L"Storage.PersistBookmarks", m_persistBookmarks);
            m_cacheProviderMetadata = readBool(L"Storage.CacheProviderMetadata", m_cacheProviderMetadata);
            m_redactComputerName = readBool(L"Privacy.RedactComputerName", m_redactComputerName);
            m_redactUserNames = readBool(L"Privacy.RedactUserNames", m_redactUserNames);
            m_rawXPathMode = readBool(L"Advanced.RawXPathMode", m_rawXPathMode);
            m_debugLogging = readBool(L"Advanced.DebugLogging", m_debugLogging);
        }
        catch (...)
        {
        }
    }

    void SettingsViewModel::SaveBoolSetting(winrt::hstring const& key, bool value) const
    {
        try { Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(key, winrt::box_value(value)); }
        catch (...) { }
    }

    void SettingsViewModel::SaveTextSetting(winrt::hstring const& key, winrt::hstring const& value) const
    {
        try { Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(key, winrt::box_value(value)); }
        catch (...) { }
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
