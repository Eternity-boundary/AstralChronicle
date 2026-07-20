// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "SavedViewItemViewModel.g.h"
#include "Models/SavedView.h"

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct SavedViewItemViewModel : SavedViewItemViewModelT<SavedViewItemViewModel>
    {
        SavedViewItemViewModel() = default;

        void Initialize(
            ::AstralChronicle::models::SavedView const& view,
            ::AstralChronicle::design::IStringResourceService const& strings);

        [[nodiscard]] winrt::hstring Id() const;
        [[nodiscard]] winrt::hstring Name() const;
        [[nodiscard]] winrt::hstring Description() const;
        [[nodiscard]] winrt::hstring Type() const;
        [[nodiscard]] winrt::hstring Channel() const;
        [[nodiscard]] winrt::hstring Query() const;
        [[nodiscard]] winrt::hstring UpdatedAt() const;
        [[nodiscard]] bool IsPinned() const noexcept;
        void IsPinned(bool value);

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::hstring m_id;
        winrt::hstring m_name;
        winrt::hstring m_description;
        winrt::hstring m_type;
        winrt::hstring m_channel;
        winrt::hstring m_query;
        winrt::hstring m_updatedAt;
        bool m_isPinned{};
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SavedViewItemViewModel : SavedViewItemViewModelT<SavedViewItemViewModel, implementation::SavedViewItemViewModel>
    {
    };
}
