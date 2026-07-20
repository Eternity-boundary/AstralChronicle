// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "FeaturePlaceholderPage.g.h"
#include "ViewModels/FeaturePlaceholderViewModel.h"

#include <optional>

namespace winrt::AstralChronicle::implementation
{
    struct FeaturePlaceholderPage : FeaturePlaceholderPageT<FeaturePlaceholderPage>
    {
        FeaturePlaceholderPage();

        [[nodiscard]] winrt::AstralChronicle::FeaturePlaceholderViewModel ViewModel() const;
        void Initialize(
            winrt::hstring const& heading,
            winrt::hstring const& description,
            std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel = std::nullopt);

    private:
        winrt::AstralChronicle::FeaturePlaceholderViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct FeaturePlaceholderPage : FeaturePlaceholderPageT<FeaturePlaceholderPage, implementation::FeaturePlaceholderPage>
    {
    };
}
