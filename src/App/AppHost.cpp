// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "AppHost.h"

#include "Core/Navigation/NavigationService.h"
#include "DesignSystem/Localization/StringResourceService.h"
#include "DesignSystem/Theme/ThemeService.h"
#include "Services/WindowsEventLogCatalogService.h"
#include "Services/WindowsEventLiveService.h"
#include "Services/WindowsEventProviderService.h"
#include "Services/WindowsEventQueryService.h"
#include "Services/WindowsRemoteEventService.h"
#include "Services/WindowsSavedViewRepository.h"
#include "Services/WindowsSessionRepository.h"

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
        m_services.AddSingleton<services::IEventLiveService>(
            std::make_shared<services::WindowsEventLiveService>());
        m_services.AddSingleton<services::IEventProviderService>(
            std::make_shared<services::WindowsEventProviderService>());
        m_services.AddSingleton<services::IEventQueryService>(
            std::make_shared<services::WindowsEventQueryService>());
        m_services.AddSingleton<services::IRemoteEventService>(
            std::make_shared<services::WindowsRemoteEventService>());
        m_services.AddSingleton<services::ISavedViewRepository>(
            std::make_shared<services::WindowsSavedViewRepository>());
        m_services.AddSingleton<services::ISessionRepository>(
            std::make_shared<services::WindowsSessionRepository>());
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

    services::IEventLiveService& AppHost::EventLive() const
    {
        return *m_services.GetRequiredService<services::IEventLiveService>();
    }

    services::IEventProviderService& AppHost::EventProviders() const
    {
        return *m_services.GetRequiredService<services::IEventProviderService>();
    }

    services::IEventQueryService& AppHost::EventQuery() const
    {
        return *m_services.GetRequiredService<services::IEventQueryService>();
    }

    services::IRemoteEventService& AppHost::RemoteEvents() const
    {
        return *m_services.GetRequiredService<services::IRemoteEventService>();
    }

    services::ISavedViewRepository& AppHost::SavedViews() const
    {
        return *m_services.GetRequiredService<services::ISavedViewRepository>();
    }

    services::ISessionRepository& AppHost::Sessions() const
    {
        return *m_services.GetRequiredService<services::ISessionRepository>();
    }
}
