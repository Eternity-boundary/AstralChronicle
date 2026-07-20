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
        [[nodiscard]] bool DefaultSortDescending() const noexcept;
        void DefaultSortDescending(bool value);
        [[nodiscard]] bool UseUtc() const noexcept;
        void UseUtc(bool value);
        [[nodiscard]] bool GroupRepeatedEvents() const noexcept;
        void GroupRepeatedEvents(bool value);
        [[nodiscard]] bool DetailsPaneOpen() const noexcept;
        void DetailsPaneOpen(bool value);
        [[nodiscard]] bool AnimationsEnabled() const noexcept;
        void AnimationsEnabled(bool value);
        [[nodiscard]] winrt::hstring LiveQueueLimit() const;
        void LiveQueueLimit(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring QueryBatchSize() const;
        void QueryBatchSize(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring MaxVisibleLiveRows() const;
        void MaxVisibleLiveRows(winrt::hstring const& value);
        [[nodiscard]] bool PersistSessions() const noexcept;
        void PersistSessions(bool value);
        [[nodiscard]] bool PersistBookmarks() const noexcept;
        void PersistBookmarks(bool value);
        [[nodiscard]] bool CacheProviderMetadata() const noexcept;
        void CacheProviderMetadata(bool value);
        [[nodiscard]] bool RedactComputerName() const noexcept;
        void RedactComputerName(bool value);
        [[nodiscard]] bool RedactUserNames() const noexcept;
        void RedactUserNames(bool value);
        [[nodiscard]] bool RawXPathMode() const noexcept;
        void RawXPathMode(bool value);
        [[nodiscard]] bool DebugLogging() const noexcept;
        void DebugLogging(bool value);

        winrt::event_token PropertyChanged(
            Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void LoadPersistedSettings();
        void SaveBoolSetting(winrt::hstring const& key, bool value) const;
        void SaveTextSetting(winrt::hstring const& key, winrt::hstring const& value) const;

        winrt::hstring m_heading;
        winrt::hstring m_description;
        winrt::hstring m_themeHint;
        std::int32_t m_selectedThemeIndex{};
        bool m_defaultSortDescending{ true };
        bool m_useUtc{};
        bool m_groupRepeatedEvents{};
        bool m_detailsPaneOpen{ true };
        bool m_animationsEnabled{ true };
        winrt::hstring m_liveQueueLimit{ L"5000" };
        winrt::hstring m_queryBatchSize{ L"64" };
        winrt::hstring m_maxVisibleLiveRows{ L"2000" };
        bool m_persistSessions{ true };
        bool m_persistBookmarks{ true };
        bool m_cacheProviderMetadata{ true };
        bool m_redactComputerName{};
        bool m_redactUserNames{};
        bool m_rawXPathMode{};
        bool m_debugLogging{};
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SettingsViewModel : SettingsViewModelT<SettingsViewModel, implementation::SettingsViewModel>
    {
    };
}
