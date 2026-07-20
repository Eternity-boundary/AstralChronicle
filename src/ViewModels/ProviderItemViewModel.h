// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "ProviderItemViewModel.g.h"

namespace winrt::AstralChronicle::implementation
{
    struct ProviderItemViewModel : ProviderItemViewModelT<ProviderItemViewModel>
    {
        ProviderItemViewModel() = default;

        void Initialize(winrt::hstring const& name);
        [[nodiscard]] winrt::hstring Name() const;

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::hstring m_name;
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct ProviderItemViewModel : ProviderItemViewModelT<ProviderItemViewModel, implementation::ProviderItemViewModel>
    {
    };
}
