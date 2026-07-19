// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace AstralChronicle::navigation
{
    using PageFactory = std::function<winrt::Microsoft::UI::Xaml::FrameworkElement()>;

    struct PageRegistration final
    {
        std::wstring Route;
        PageFactory CreatePage;
    };

    struct INavigationService
    {
        virtual ~INavigationService() = default;

        virtual void Attach(winrt::Microsoft::UI::Xaml::Controls::Frame const& frame) = 0;
        virtual void Register(PageRegistration registration) = 0;
        [[nodiscard]] virtual bool Navigate(std::wstring_view route) = 0;
    };
}
