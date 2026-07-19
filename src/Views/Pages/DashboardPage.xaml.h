// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DashboardPage.g.h"
#include "ViewModels/DashboardViewModel.h"

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
    struct DashboardPage : DashboardPageT<DashboardPage>
    {
        DashboardPage();

        [[nodiscard]] winrt::AstralChronicle::DashboardViewModel ViewModel() const;
        void Initialize(
            ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings);

    private:
        winrt::AstralChronicle::DashboardViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage, implementation::DashboardPage>
    {
    };
}
