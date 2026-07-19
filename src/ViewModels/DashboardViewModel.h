// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DashboardViewModel.g.h"

namespace AstralChronicle::services
{
    struct IEventLogCatalogService;
    struct IEventQueryService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct DashboardViewModel : DashboardViewModelT<DashboardViewModel>
    {
        DashboardViewModel();

        void Initialize(
            ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring ChannelCountLabel() const;
        [[nodiscard]] winrt::hstring RecentEventPreview() const;
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(winrt::hstring const& propertyName);

        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_channelCountLabel;
        winrt::hstring m_recentEventPreview;
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct DashboardViewModel : DashboardViewModelT<DashboardViewModel, implementation::DashboardViewModel>
    {
    };
}
