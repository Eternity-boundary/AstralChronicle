// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "FeaturePlaceholderViewModel.h"

#include "FeaturePlaceholderViewModel.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    FeaturePlaceholderViewModel::FeaturePlaceholderViewModel()
    {
    }

    void FeaturePlaceholderViewModel::Initialize(winrt::hstring const& heading, winrt::hstring const& description)
    {
        m_heading = heading;
        m_description = description;
        RaisePropertyChanged(L"Heading");
        RaisePropertyChanged(L"Description");
    }

    winrt::hstring FeaturePlaceholderViewModel::Heading() const
    {
        return m_heading;
    }

    winrt::hstring FeaturePlaceholderViewModel::Description() const
    {
        return m_description;
    }

    winrt::event_token FeaturePlaceholderViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void FeaturePlaceholderViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void FeaturePlaceholderViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
