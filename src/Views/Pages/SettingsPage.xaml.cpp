// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SettingsPage.xaml.h"

#include "SettingsPage.g.cpp"

#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.Globalization.h>

#include <algorithm>
#include <string_view>

namespace winrt::AstralChronicle::implementation
{
    namespace
    {
        constexpr std::int32_t EnglishLanguageIndex = 0;
        constexpr std::int32_t TraditionalChineseLanguageIndex = 1;

        hstring LanguageTagForIndex(std::int32_t const index)
        {
            return index == TraditionalChineseLanguageIndex ? L"zh-TW" : L"en-US";
        }

        std::int32_t CurrentLanguageIndex()
        {
            auto language = Microsoft::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride();
            if (language.empty())
            {
                auto const languages = Microsoft::Windows::Globalization::ApplicationLanguages::Languages();
                if (languages.Size() > 0)
                {
                    language = languages.GetAt(0);
                }
            }

            auto const languageView = std::wstring_view{ language.c_str(), language.size() };
            return languageView.starts_with(L"zh")
                ? TraditionalChineseLanguageIndex
                : EnglishLanguageIndex;
        }
    }

    SettingsPage::SettingsPage()
        : m_viewModel(winrt::make<SettingsViewModel>())
    {
        InitializeComponent();
    }

    SettingsPage::~SettingsPage()
    {
        if (m_theme && m_themeSubscriptionId != 0)
        {
            m_theme->Unsubscribe(m_themeSubscriptionId);
        }
    }

    winrt::AstralChronicle::SettingsViewModel SettingsPage::ViewModel() const
    {
        return m_viewModel;
    }

    void SettingsPage::Initialize(
        ::AstralChronicle::design::IThemeService& theme,
        ::AstralChronicle::design::IStringResourceService const& strings)
    {
        m_theme = &theme;
        m_isInitializing = true;
        winrt::get_self<SettingsViewModel>(m_viewModel)->Initialize(
            static_cast<std::int32_t>(theme.CurrentMode()),
            strings.GetString(L"Settings.Heading"),
            strings.GetString(L"Settings.Description"),
            strings.GetString(L"Settings.ThemeHint"));
        LanguageRadioButtons().SelectedIndex(CurrentLanguageIndex());
        m_isInitializing = false;

        m_themeSubscriptionId = m_theme->Subscribe(
            [this](::AstralChronicle::design::ThemeMode mode)
            {
                winrt::get_self<SettingsViewModel>(m_viewModel)->SelectedThemeIndex(static_cast<std::int32_t>(mode));
            });
    }

    void SettingsPage::OnThemeSelectionChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        if (m_isInitializing || !m_theme)
        {
            return;
        }

        auto const radioButtons = sender.try_as<Microsoft::UI::Xaml::Controls::RadioButtons>();
        if (!radioButtons)
        {
            return;
        }

        auto const index = radioButtons.SelectedIndex();
        if (index >= 0)
        {
            m_theme->SetMode(
                static_cast<::AstralChronicle::design::ThemeMode>(index));
        }
    }

    void SettingsPage::OnLanguageSelectionChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        if (m_isInitializing)
        {
            return;
        }

        auto const radioButtons = sender.try_as<Microsoft::UI::Xaml::Controls::RadioButtons>();
        if (!radioButtons)
        {
            return;
        }

        auto const index = radioButtons.SelectedIndex();
        if (index != EnglishLanguageIndex && index != TraditionalChineseLanguageIndex)
        {
            return;
        }

        if (index == CurrentLanguageIndex())
        {
            return;
        }

        Microsoft::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride(
            LanguageTagForIndex(index));

        // Restart only returns when it fails. Preserve the failure reason for
        // diagnostics instead of silently leaving the current UI unchanged.
        auto const restartFailure = Microsoft::Windows::AppLifecycle::AppInstance::Restart(L"");
        auto const diagnostic = std::wstring{ L"Language switch restart failed. Reason: " }
            + std::to_wstring(static_cast<std::int32_t>(restartFailure));
        OutputDebugStringW(diagnostic.c_str());
    }

    void SettingsPage::OnPageRootSizeChanged(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
    {
        auto const padding = PageRoot().Padding();
        auto const contentWidth = std::max(0.0, args.NewSize().Width - padding.Left - padding.Right);
        PageContent().Width(contentWidth);
    }
}
