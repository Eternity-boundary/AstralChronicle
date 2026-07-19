// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <functional>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace AstralChronicle::design
{
    enum class ThemeMode : std::int32_t
    {
        System = 0,
        Light = 1,
        Dark = 2,
        StarrySky = 3,
        BlackSoulsLeaf = 4,
    };

    struct ThemeBackdrop final
    {
        winrt::Microsoft::UI::Xaml::Media::Brush MainImage{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Brush NavigationImage{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Brush MainContrastOverlay{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Brush NavigationContrastOverlay{ nullptr };
    };

    struct IThemeService
    {
        using ThemeChangedCallback = std::function<void(ThemeMode)>;

        virtual ~IThemeService() = default;

        virtual void Initialize(winrt::Microsoft::UI::Xaml::FrameworkElement const& rootElement) = 0;
        virtual ThemeMode CurrentMode() const noexcept = 0;
        virtual void SetMode(ThemeMode mode) = 0;
        [[nodiscard]] virtual ThemeBackdrop Backdrop() const = 0;
        virtual std::uint32_t Subscribe(ThemeChangedCallback callback) = 0;
        virtual void Unsubscribe(std::uint32_t subscriptionId) = 0;
    };
}
