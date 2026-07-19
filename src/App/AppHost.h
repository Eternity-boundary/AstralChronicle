// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Core/DependencyInjection/ServiceProvider.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Core/Navigation/INavigationService.h"
#include "DesignSystem/Theme/IThemeService.h"
#include "Services/IEventLogCatalogService.h"
#include "Services/IEventQueryService.h"

namespace AstralChronicle::app
{
    class AppHost final
    {
    public:
        AppHost();

        [[nodiscard]] core::ServiceProvider const& Services() const noexcept;
        [[nodiscard]] design::IStringResourceService& Strings() const;
        [[nodiscard]] navigation::INavigationService& Navigation() const;
        [[nodiscard]] design::IThemeService& Theme() const;
        [[nodiscard]] services::IEventLogCatalogService& EventLogCatalog() const;
        [[nodiscard]] services::IEventQueryService& EventQuery() const;

    private:
        core::ServiceProvider m_services;
    };
}
