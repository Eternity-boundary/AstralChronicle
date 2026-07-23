// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DesignSystem/Theme/ThemeService.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace AstralChronicle::design
{
    namespace
    {
        constexpr wchar_t ThemeModeSetting[] = L"ThemeMode";
    }

    ThemeService::ThemeService()
    {
        // The EtherealScepter theme service is the source pattern for this
        // service. AstralChronicle deliberately keeps the lifecycle and
        // subscription model. Artwork modes use bounded, named overlay
        // brushes here; all ordinary surfaces and High Contrast colors remain
        // owned by the semantic DesignSystem dictionaries.
        LoadMode();
    }

    ThemeService::~ThemeService()
    {
        Detach();
    }

    void ThemeService::Initialize(FrameworkElement const& rootElement)
    {
        {
            std::lock_guard lock(m_mutex);
            m_rootElement = rootElement;
        }

        try
        {
            if (!m_accessibilitySettings)
            {
                m_accessibilitySettings = Windows::UI::ViewManagement::AccessibilitySettings{};
                auto const weak = weak_from_this();
                m_highContrastChangedToken = m_accessibilitySettings.HighContrastChanged(
                    [weak](auto const&, auto const&)
                    {
                        if (auto const self = weak.lock())
                        {
                            self->ApplyMode();
                            self->NotifyThemeChanged(self->CurrentMode());
                        }
                    });
            }
        }
        catch (...)
        {
            // Some design-time hosts do not expose AccessibilitySettings.
        }

        ApplyMode();
    }

    void ThemeService::Detach() noexcept
    {
        auto accessibilitySettings = m_accessibilitySettings;
        auto const token = m_highContrastChangedToken;
        m_highContrastChangedToken = {};
        m_accessibilitySettings = nullptr;

        try
        {
            if (accessibilitySettings && token.value != 0)
            {
                accessibilitySettings.HighContrastChanged(token);
            }
        }
        catch (...)
        {
        }

        std::lock_guard lock(m_mutex);
        m_rootElement = nullptr;
        m_subscribers.clear();
        m_starrySkyBackdrop = {};
        m_blackSoulsLeafBackdrop = {};
        m_starrySkyBackdropInitialized = false;
        m_blackSoulsLeafBackdropInitialized = false;
    }

    ThemeMode ThemeService::CurrentMode() const noexcept
    {
        std::lock_guard lock(m_mutex);
        return m_mode;
    }

    void ThemeService::SetMode(ThemeMode mode)
    {
        if (!IsValidMode(mode))
        {
            return;
        }

        bool changed = false;
        {
            std::lock_guard lock(m_mutex);
            if (m_mode != mode)
            {
                m_mode = mode;
                changed = true;
            }
        }

        if (!changed)
        {
            return;
        }

        ApplyMode();
        SaveMode();
        NotifyThemeChanged(mode);
    }

    ThemeBackdrop ThemeService::Backdrop() const
    {
        ThemeMode mode{ ThemeMode::System };
        {
            std::lock_guard lock(m_mutex);
            mode = m_mode;
        }

        // High Contrast owns the complete visual contract. Hiding imagery in
        // that mode prevents a photograph from competing with system colors.
        if (!IsCustomMode(mode) || IsHighContrastActive())
        {
            return {};
        }

        switch (mode)
        {
        case ThemeMode::StarrySky:
        {
            {
                std::lock_guard lock(m_mutex);
                if (m_starrySkyBackdropInitialized)
                {
                    return m_starrySkyBackdrop;
                }
            }
            auto backdrop = ThemeBackdrop{
                CreateImageBrush(L"ms-appx:///Assets/StarrySkyMain.png"),
                CreateImageBrush(L"ms-appx:///Assets/StarrySkySide.png"),
                CreateOverlayBrush(2, 8, 24, 0.55),
                CreateOverlayBrush(0, 8, 18, 0.72) };
            std::lock_guard lock(m_mutex);
            if (!m_starrySkyBackdropInitialized)
            {
                m_starrySkyBackdrop = std::move(backdrop);
                m_starrySkyBackdropInitialized = true;
            }
            return m_starrySkyBackdrop;
        }
        case ThemeMode::BlackSoulsLeaf:
        {
            {
                std::lock_guard lock(m_mutex);
                if (m_blackSoulsLeafBackdropInitialized)
                {
                    return m_blackSoulsLeafBackdrop;
                }
            }
            auto backdrop = ThemeBackdrop{
                CreateImageBrush(L"ms-appx:///Assets/LeafMain.png"),
                CreateImageBrush(L"ms-appx:///Assets/LeafSide.png"),
                CreateOverlayBrush(9, 20, 10, 0.68),
                CreateOverlayBrush(10, 28, 13, 0.78) };
            std::lock_guard lock(m_mutex);
            if (!m_blackSoulsLeafBackdropInitialized)
            {
                m_blackSoulsLeafBackdrop = std::move(backdrop);
                m_blackSoulsLeafBackdropInitialized = true;
            }
            return m_blackSoulsLeafBackdrop;
        }
        default:
            return {};
        }
    }

    std::uint32_t ThemeService::Subscribe(ThemeChangedCallback callback)
    {
        if (!callback)
        {
            return 0;
        }

        std::lock_guard lock(m_mutex);
        auto const subscriptionId = m_nextSubscriptionId++;
        m_subscribers.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void ThemeService::Unsubscribe(std::uint32_t subscriptionId)
    {
        if (subscriptionId == 0)
        {
            return;
        }

        std::lock_guard lock(m_mutex);
        m_subscribers.erase(subscriptionId);
    }

    bool ThemeService::IsValidMode(ThemeMode mode) noexcept
    {
        return mode == ThemeMode::System || mode == ThemeMode::Light || mode == ThemeMode::Dark ||
               mode == ThemeMode::StarrySky || mode == ThemeMode::BlackSoulsLeaf;
    }

    bool ThemeService::IsCustomMode(ThemeMode mode) noexcept
    {
        return mode == ThemeMode::StarrySky || mode == ThemeMode::BlackSoulsLeaf;
    }

    bool ThemeService::IsHighContrastActive() noexcept
    {
        try
        {
            return Windows::UI::ViewManagement::AccessibilitySettings{}.HighContrast();
        }
        catch (...)
        {
            return false;
        }
    }

    Microsoft::UI::Xaml::Media::Brush ThemeService::CreateImageBrush(std::wstring_view path)
    {
        if (path.empty())
        {
            return nullptr;
        }

        try
        {
            Microsoft::UI::Xaml::Media::ImageBrush brush;
            Microsoft::UI::Xaml::Media::Imaging::BitmapImage bitmap;
            bitmap.UriSource(Windows::Foundation::Uri{ winrt::hstring{ path } });
            brush.ImageSource(bitmap);
            brush.Stretch(Microsoft::UI::Xaml::Media::Stretch::UniformToFill);
            return brush;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    Microsoft::UI::Xaml::Media::Brush ThemeService::CreateOverlayBrush(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        double opacity)
    {
        Microsoft::UI::Xaml::Media::SolidColorBrush brush;
        Windows::UI::Color color{};
        color.A = 255;
        color.R = red;
        color.G = green;
        color.B = blue;
        brush.Color(color);
        brush.Opacity(opacity);
        return brush;
    }

    void ThemeService::LoadMode()
    {
        try
        {
            auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            if (!values.HasKey(ThemeModeSetting))
            {
                return;
            }

            auto const mode = static_cast<ThemeMode>(unbox_value<std::int32_t>(values.Lookup(ThemeModeSetting)));
            if (IsValidMode(mode))
            {
                m_mode = mode;
            }
        }
        catch (...)
        {
            // A first launch, an unavailable package store, or a stale value
            // should leave the application on the safe System default.
        }
    }

    void ThemeService::SaveMode() const
    {
        try
        {
            auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            std::lock_guard lock(m_mutex);
            values.Insert(ThemeModeSetting, box_value(static_cast<std::int32_t>(m_mode)));
        }
        catch (...)
        {
            // Theme persistence is best effort and must not affect navigation.
        }
    }

    void ThemeService::ApplyMode() const
    {
        FrameworkElement rootElement{ nullptr };
        ThemeMode mode{ ThemeMode::System };
        {
            std::lock_guard lock(m_mutex);
            rootElement = m_rootElement;
            mode = m_mode;
        }

        if (!rootElement)
        {
            return;
        }

        switch (mode)
        {
        case ThemeMode::Light:
            rootElement.RequestedTheme(ElementTheme::Light);
            break;
        case ThemeMode::Dark:
            rootElement.RequestedTheme(ElementTheme::Dark);
            break;
        case ThemeMode::StarrySky:
        case ThemeMode::BlackSoulsLeaf:
            rootElement.RequestedTheme(IsHighContrastActive() ? ElementTheme::Default : ElementTheme::Dark);
            break;
        case ThemeMode::System:
        default:
            // Default follows Windows, including High Contrast mode. The
            // resource dictionaries provide an explicit HighContrast branch.
            rootElement.RequestedTheme(ElementTheme::Default);
            break;
        }
    }

    void ThemeService::NotifyThemeChanged(ThemeMode mode) const
    {
        std::map<std::uint32_t, ThemeChangedCallback> subscribers;
        {
            std::lock_guard lock(m_mutex);
            subscribers = m_subscribers;
        }

        for (auto const& [subscriptionId, callback] : subscribers)
        {
            (void)subscriptionId;
            if (callback)
            {
                callback(mode);
            }
        }
    }
}
