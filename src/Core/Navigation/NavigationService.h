// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "INavigationService.h"

#include <unordered_map>

namespace AstralChronicle::navigation
{
    class NavigationService final : public INavigationService
    {
    public:
        void Attach(winrt::Microsoft::UI::Xaml::Controls::Frame const& frame) override;
        void Register(PageRegistration registration) override;
        [[nodiscard]] bool Navigate(std::wstring_view route) override;

    private:
        winrt::Microsoft::UI::Xaml::Controls::Frame m_frame{ nullptr };
        std::unordered_map<std::wstring, PageFactory> m_factories;
    };
}
