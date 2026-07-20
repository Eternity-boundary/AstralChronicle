// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "ProviderItemViewModel.h"

#include "ProviderItemViewModel.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    void ProviderItemViewModel::Initialize(winrt::hstring const& name)
    {
        m_name = name;
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Name" });
    }

    winrt::hstring ProviderItemViewModel::Name() const
    {
        return m_name;
    }

    winrt::event_token ProviderItemViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void ProviderItemViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }
}
