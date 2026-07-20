// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventChannelDescriptor.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace AstralChronicle::navigation
{
    struct NavigationRequest final
    {
        std::wstring Route;
        std::optional<models::EventChannelIdentifier> Channel;
        std::optional<std::wstring> Query;
    };

    using PageFactory = std::function<winrt::Microsoft::UI::Xaml::FrameworkElement()>;
    using ParameterizedPageFactory = std::function<winrt::Microsoft::UI::Xaml::FrameworkElement(
        NavigationRequest const&)>;

    struct PageRegistration final
    {
        std::wstring Route;
        PageFactory CreatePage;
        ParameterizedPageFactory CreatePageWithRequest;
    };

    struct INavigationService
    {
        virtual ~INavigationService() = default;

        virtual void Attach(winrt::Microsoft::UI::Xaml::Controls::Frame const& frame) = 0;
        virtual void Register(PageRegistration registration) = 0;
        [[nodiscard]] virtual bool Navigate(std::wstring_view route) = 0;
        [[nodiscard]] virtual bool Navigate(NavigationRequest const& request) = 0;
    };
}
