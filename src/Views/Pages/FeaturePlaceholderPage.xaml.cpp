// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "FeaturePlaceholderPage.xaml.h"

#include "FeaturePlaceholderPage.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    FeaturePlaceholderPage::FeaturePlaceholderPage()
        : m_viewModel(winrt::make<FeaturePlaceholderViewModel>())
    {
    }

    winrt::AstralChronicle::FeaturePlaceholderViewModel FeaturePlaceholderPage::ViewModel() const
    {
        return m_viewModel;
    }

    void FeaturePlaceholderPage::Initialize(winrt::hstring const& heading, winrt::hstring const& description)
    {
        winrt::get_self<FeaturePlaceholderViewModel>(m_viewModel)->Initialize(heading, description);
    }
}
