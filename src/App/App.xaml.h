// Created by EternityBoundary on Jul 19,2026
#pragma once

#include "App.xaml.g.h"

namespace winrt::AstralChronicle::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
    };
}
