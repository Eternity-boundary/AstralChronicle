// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "FeaturePlaceholderViewModel.g.h"

namespace winrt::AstralChronicle::implementation
{
    struct FeaturePlaceholderViewModel : FeaturePlaceholderViewModelT<FeaturePlaceholderViewModel>
    {
        FeaturePlaceholderViewModel();

        void Initialize(winrt::hstring const& heading, winrt::hstring const& description);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Description() const;
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(winrt::hstring const& propertyName);

        winrt::hstring m_heading;
        winrt::hstring m_description;
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct FeaturePlaceholderViewModel : FeaturePlaceholderViewModelT<FeaturePlaceholderViewModel, implementation::FeaturePlaceholderViewModel>
    {
    };
}
