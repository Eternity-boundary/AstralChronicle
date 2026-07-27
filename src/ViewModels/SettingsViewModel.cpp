// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SettingsViewModel.h"

#include "SettingsViewModel.g.cpp"

#include <cstdint>
#include <optional>

namespace
{
    [[nodiscard]] std::optional<winrt::hstring> NormalizePositiveInteger(
        winrt::hstring const& text,
        std::uint32_t const minimum,
        std::uint32_t const maximum)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::uint64_t value{};
        for (auto const character : text)
        {
            if (character < L'0' || character > L'9')
            {
                return std::nullopt;
            }
            value = value * 10 + static_cast<std::uint64_t>(character - L'0');
            if (value > maximum)
            {
                return std::nullopt;
            }
        }

        if (value < minimum)
        {
            return std::nullopt;
        }
        return winrt::to_hstring(value);
    }
}

namespace winrt::AstralChronicle::implementation
{
    void SettingsViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IApplicationPreferencesService> preferences,
        std::int32_t selectedThemeIndex,
        winrt::hstring const& heading,
        winrt::hstring const& description,
        winrt::hstring const& themeHint)
    {
        m_preferences = std::move(preferences);
        if (!m_preferences)
        {
            throw std::invalid_argument("Settings require a preferences service.");
        }
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
        m_preferences->WriteBool(L"EventDisplay.DefaultSortDescending", value);
        RaisePropertyChanged(L"DefaultSortDescending");
    }

    bool SettingsViewModel::UseUtc() const noexcept { return m_useUtc; }
    void SettingsViewModel::UseUtc(bool value)
    {
        if (m_useUtc == value) return;
        m_useUtc = value;
        m_preferences->WriteBool(L"EventDisplay.UseUtc", value);
        RaisePropertyChanged(L"UseUtc");
    }

    bool SettingsViewModel::GroupRepeatedEvents() const noexcept { return m_groupRepeatedEvents; }
    void SettingsViewModel::GroupRepeatedEvents(bool value)
    {
        if (m_groupRepeatedEvents == value) return;
        m_groupRepeatedEvents = value;
        m_preferences->WriteBool(L"EventDisplay.GroupRepeatedEvents", value);
        RaisePropertyChanged(L"GroupRepeatedEvents");
    }

    bool SettingsViewModel::DetailsPaneOpen() const noexcept { return m_detailsPaneOpen; }
    void SettingsViewModel::DetailsPaneOpen(bool value)
    {
        if (m_detailsPaneOpen == value) return;
        m_detailsPaneOpen = value;
        m_preferences->WriteBool(L"EventDisplay.DetailsPaneOpen", value);
        RaisePropertyChanged(L"DetailsPaneOpen");
    }

    bool SettingsViewModel::AnimationsEnabled() const noexcept { return m_animationsEnabled; }
    void SettingsViewModel::AnimationsEnabled(bool value)
    {
        if (m_animationsEnabled == value) return;
        m_animationsEnabled = value;
        m_preferences->WriteBool(L"Appearance.AnimationsEnabled", value);
        RaisePropertyChanged(L"AnimationsEnabled");
    }

    winrt::hstring SettingsViewModel::LiveQueueLimit() const { return m_liveQueueLimit; }
    void SettingsViewModel::LiveQueueLimit(winrt::hstring const& value)
    {
        if (m_liveQueueLimit == value) return;
        m_liveQueueLimit = value;
        if (auto const normalized = NormalizePositiveInteger(value, 64, 100'000))
        {
            m_liveQueueLimit = *normalized;
            m_preferences->WriteText(L"Monitoring.LiveQueueLimit", m_liveQueueLimit);
        }
        RaisePropertyChanged(L"LiveQueueLimit");
    }

    winrt::hstring SettingsViewModel::QueryBatchSize() const { return m_queryBatchSize; }
    void SettingsViewModel::QueryBatchSize(winrt::hstring const& value)
    {
        if (m_queryBatchSize == value) return;
        m_queryBatchSize = value;
        if (auto const normalized = NormalizePositiveInteger(value, 1, 4'096))
        {
            m_queryBatchSize = *normalized;
            m_preferences->WriteText(L"Performance.QueryBatchSize", m_queryBatchSize);
        }
        RaisePropertyChanged(L"QueryBatchSize");
    }

    winrt::hstring SettingsViewModel::MaxVisibleLiveRows() const { return m_maxVisibleLiveRows; }
    void SettingsViewModel::MaxVisibleLiveRows(winrt::hstring const& value)
    {
        if (m_maxVisibleLiveRows == value) return;
        m_maxVisibleLiveRows = value;
        if (auto const normalized = NormalizePositiveInteger(value, 64, 100'000))
        {
            m_maxVisibleLiveRows = *normalized;
            m_preferences->WriteText(L"Performance.MaxVisibleLiveRows", m_maxVisibleLiveRows);
        }
        RaisePropertyChanged(L"MaxVisibleLiveRows");
    }

    bool SettingsViewModel::PersistSessions() const noexcept { return m_persistSessions; }
    void SettingsViewModel::PersistSessions(bool value)
    {
        if (m_persistSessions == value) return;
        m_persistSessions = value;
        m_preferences->WriteBool(L"Storage.PersistSessions", value);
        if (!value)
        {
            m_preferences->ClearPersistedSessions();
        }
        RaisePropertyChanged(L"PersistSessions");
    }

    bool SettingsViewModel::PersistBookmarks() const noexcept { return m_persistBookmarks; }
    void SettingsViewModel::PersistBookmarks(bool value)
    {
        if (m_persistBookmarks == value) return;
        m_persistBookmarks = value;
        m_preferences->WriteBool(L"Storage.PersistBookmarks", value);
        if (!value)
        {
            m_preferences->ClearPersistedBookmarks();
        }
        RaisePropertyChanged(L"PersistBookmarks");
    }

    bool SettingsViewModel::CacheProviderMetadata() const noexcept { return m_cacheProviderMetadata; }
    void SettingsViewModel::CacheProviderMetadata(bool value)
    {
        if (m_cacheProviderMetadata == value) return;
        m_cacheProviderMetadata = value;
        m_preferences->WriteBool(L"Storage.CacheProviderMetadata", value);
        RaisePropertyChanged(L"CacheProviderMetadata");
    }

    bool SettingsViewModel::RedactComputerName() const noexcept { return m_redactComputerName; }
    void SettingsViewModel::RedactComputerName(bool value)
    {
        if (m_redactComputerName == value) return;
        m_redactComputerName = value;
        m_preferences->WriteBool(L"Privacy.RedactComputerName", value);
        RaisePropertyChanged(L"RedactComputerName");
    }

    bool SettingsViewModel::RedactUserNames() const noexcept { return m_redactUserNames; }
    void SettingsViewModel::RedactUserNames(bool value)
    {
        if (m_redactUserNames == value) return;
        m_redactUserNames = value;
        m_preferences->WriteBool(L"Privacy.RedactUserNames", value);
        RaisePropertyChanged(L"RedactUserNames");
    }

    bool SettingsViewModel::RawXPathMode() const noexcept { return m_rawXPathMode; }
    void SettingsViewModel::RawXPathMode(bool value)
    {
        if (m_rawXPathMode == value) return;
        m_rawXPathMode = value;
        m_preferences->WriteBool(L"Advanced.RawXPathMode", value);
        RaisePropertyChanged(L"RawXPathMode");
    }

    bool SettingsViewModel::DebugLogging() const noexcept { return m_debugLogging; }
    void SettingsViewModel::DebugLogging(bool value)
    {
        if (m_debugLogging == value) return;
        m_debugLogging = value;
        m_preferences->WriteBool(L"Advanced.DebugLogging", value);
        RaisePropertyChanged(L"DebugLogging");
    }

    void SettingsViewModel::LoadPersistedSettings()
    {
        m_defaultSortDescending = m_preferences->ReadBool(
            L"EventDisplay.DefaultSortDescending", m_defaultSortDescending);
        m_useUtc = m_preferences->ReadBool(L"EventDisplay.UseUtc", m_useUtc);
        m_groupRepeatedEvents = m_preferences->ReadBool(
            L"EventDisplay.GroupRepeatedEvents", m_groupRepeatedEvents);
        m_detailsPaneOpen = m_preferences->ReadBool(
            L"EventDisplay.DetailsPaneOpen", m_detailsPaneOpen);
        m_animationsEnabled = m_preferences->ReadBool(
            L"Appearance.AnimationsEnabled", m_animationsEnabled);
        m_liveQueueLimit = winrt::to_hstring(m_preferences->ReadUInt32(
            L"Monitoring.LiveQueueLimit", 5'000, 64, 100'000));
        m_queryBatchSize = winrt::to_hstring(m_preferences->ReadUInt32(
            L"Performance.QueryBatchSize", 64, 1, 4'096));
        m_maxVisibleLiveRows = winrt::to_hstring(m_preferences->ReadUInt32(
            L"Performance.MaxVisibleLiveRows", 2'000, 64, 100'000));
        m_persistSessions = m_preferences->ReadBool(L"Storage.PersistSessions", m_persistSessions);
        m_persistBookmarks = m_preferences->ReadBool(L"Storage.PersistBookmarks", m_persistBookmarks);
        m_cacheProviderMetadata = m_preferences->ReadBool(
            L"Storage.CacheProviderMetadata", m_cacheProviderMetadata);
        m_redactComputerName = m_preferences->ReadBool(
            L"Privacy.RedactComputerName", m_redactComputerName);
        m_redactUserNames = m_preferences->ReadBool(L"Privacy.RedactUserNames", m_redactUserNames);
        m_rawXPathMode = m_preferences->ReadBool(L"Advanced.RawXPathMode", m_rawXPathMode);
        m_debugLogging = m_preferences->ReadBool(L"Advanced.DebugLogging", m_debugLogging);
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
