// Created by EternityBoundary on Jul 19,2026
#pragma once

#include "App.xaml.g.h"
#include "AppHost.h"

#include <memory>

namespace winrt::AstralChronicle::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        std::unique_ptr<::AstralChronicle::app::AppHost> m_host;
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
    };
}
