// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardPage.xaml.h"

#include "DashboardPage.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    DashboardPage::DashboardPage()
        : m_viewModel(winrt::make<DashboardViewModel>())
    {
    }

    winrt::AstralChronicle::DashboardViewModel DashboardPage::ViewModel() const
    {
        return m_viewModel;
    }

    void DashboardPage::Initialize(
        ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings)
    {
        winrt::get_self<DashboardViewModel>(m_viewModel)->Initialize(eventLogCatalog, eventQuery, strings);
    }
}
