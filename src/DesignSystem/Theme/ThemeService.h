// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DesignSystem/Theme/IThemeService.h"

#include <map>
#include <mutex>
#include <string_view>

#include <winrt/Windows.UI.ViewManagement.h>

namespace AstralChronicle::design
{
    class ThemeService final : public IThemeService
    {
    public:
        ThemeService();
        ~ThemeService();

        void Initialize(winrt::Microsoft::UI::Xaml::FrameworkElement const& rootElement) override;
        ThemeMode CurrentMode() const noexcept override;
        void SetMode(ThemeMode mode) override;
        [[nodiscard]] ThemeBackdrop Backdrop() const override;
        std::uint32_t Subscribe(ThemeChangedCallback callback) override;
        void Unsubscribe(std::uint32_t subscriptionId) override;

    private:
        static bool IsValidMode(ThemeMode mode) noexcept;
        static bool IsCustomMode(ThemeMode mode) noexcept;
        static bool IsHighContrastActive() noexcept;

        static winrt::Microsoft::UI::Xaml::Media::Brush CreateImageBrush(std::wstring_view path);
        static winrt::Microsoft::UI::Xaml::Media::Brush CreateOverlayBrush(
            std::uint8_t red,
            std::uint8_t green,
            std::uint8_t blue,
            double opacity);

        void LoadMode();
        void SaveMode() const;
        void ApplyMode() const;
        void NotifyThemeChanged(ThemeMode mode) const;

        mutable std::mutex m_mutex;
        ThemeMode m_mode{ ThemeMode::System };
        winrt::Microsoft::UI::Xaml::FrameworkElement m_rootElement{ nullptr };
        winrt::Windows::UI::ViewManagement::AccessibilitySettings m_accessibilitySettings{ nullptr };
        winrt::event_token m_highContrastChangedToken{};
        mutable std::map<std::uint32_t, ThemeChangedCallback> m_subscribers;
        mutable std::uint32_t m_nextSubscriptionId{ 1 };
    };
}
