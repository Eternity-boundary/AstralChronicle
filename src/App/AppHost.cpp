// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "AppHost.h"

#include "Core/Navigation/NavigationService.h"
#include "DesignSystem/Localization/StringResourceService.h"
#include "DesignSystem/Theme/ThemeService.h"
#include "Services/WindowsEventLogCatalogService.h"
#include "Services/WindowsEventQueryService.h"

#include <memory>

namespace AstralChronicle::app
{
    AppHost::AppHost()
    {
        m_services.AddSingleton<design::IStringResourceService>(
            std::make_shared<design::StringResourceService>());
        m_services.AddSingleton<design::IThemeService>(
            std::make_shared<design::ThemeService>());
        m_services.AddSingleton<services::IEventLogCatalogService>(
            std::make_shared<services::WindowsEventLogCatalogService>());
        m_services.AddSingleton<services::IEventQueryService>(
            std::make_shared<services::WindowsEventQueryService>());
        m_services.AddSingleton<navigation::INavigationService>(
            std::make_shared<navigation::NavigationService>());
    }

    core::ServiceProvider const& AppHost::Services() const noexcept
    {
        return m_services;
    }

    design::IStringResourceService& AppHost::Strings() const
    {
        return *m_services.GetRequiredService<design::IStringResourceService>();
    }

    navigation::INavigationService& AppHost::Navigation() const
    {
        return *m_services.GetRequiredService<navigation::INavigationService>();
    }

    design::IThemeService& AppHost::Theme() const
    {
        return *m_services.GetRequiredService<design::IThemeService>();
    }

    services::IEventLogCatalogService& AppHost::EventLogCatalog() const
    {
        return *m_services.GetRequiredService<services::IEventLogCatalogService>();
    }

    services::IEventQueryService& AppHost::EventQuery() const
    {
        return *m_services.GetRequiredService<services::IEventQueryService>();
    }
}
