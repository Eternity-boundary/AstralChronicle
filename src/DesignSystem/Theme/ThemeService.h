// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DesignSystem/Theme/IThemeService.h"

#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.UI.Dispatching.h>

namespace AstralChronicle::design
{
    class ThemeService final : public IThemeService, public std::enable_shared_from_this<ThemeService>
    {
    public:
        ThemeService();
        ~ThemeService();

        void Initialize(winrt::Microsoft::UI::Xaml::FrameworkElement const& rootElement) override;
        void Detach() noexcept override;
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
        void OnHighContrastChanged() noexcept;
        void ApplyMode() const;
        void NotifyThemeChanged(ThemeMode mode) const;

        mutable std::mutex m_mutex;
        ThemeMode m_mode{ ThemeMode::System };
        winrt::Microsoft::UI::Xaml::FrameworkElement m_rootElement{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        winrt::Windows::UI::ViewManagement::AccessibilitySettings m_accessibilitySettings{ nullptr };
        winrt::event_token m_highContrastChangedToken{};
        mutable std::map<std::uint32_t, ThemeChangedCallback> m_subscribers;
        mutable std::uint32_t m_nextSubscriptionId{ 1 };
        mutable ThemeBackdrop m_starrySkyBackdrop;
        mutable ThemeBackdrop m_blackSoulsLeafBackdrop;
        mutable bool m_starrySkyBackdropInitialized{};
        mutable bool m_blackSoulsLeafBackdropInitialized{};
    };
}
