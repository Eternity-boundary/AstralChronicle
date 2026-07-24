// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Core/DependencyInjection/ServiceProvider.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Core/Navigation/INavigationService.h"
#include "DesignSystem/Theme/IThemeService.h"
#include "Services/IEventLogCatalogService.h"
#include "Services/ICustomViewCatalogService.h"
#include "Services/IEventLiveService.h"
#include "Services/IEventProviderService.h"
#include "Services/IEventQueryService.h"
#include "Services/IRemoteEventService.h"
#include "Services/ISavedViewRepository.h"
#include "Services/ISessionRepository.h"

namespace AstralChronicle::app
{
    class AppHost final
    {
    public:
        AppHost();
        ~AppHost();

        [[nodiscard]] core::ServiceProvider const& Services() const noexcept;
        [[nodiscard]] design::IStringResourceService& Strings() const;
        [[nodiscard]] navigation::INavigationService& Navigation() const;
        [[nodiscard]] design::IThemeService& Theme() const;
        [[nodiscard]] services::IEventLogCatalogService& EventLogCatalog() const;
        [[nodiscard]] services::ICustomViewCatalogService& CustomViews() const;
        [[nodiscard]] services::IEventLiveService& EventLive() const;
        [[nodiscard]] services::IEventProviderService& EventProviders() const;
        [[nodiscard]] services::IEventQueryService& EventQuery() const;
        [[nodiscard]] services::IRemoteEventService& RemoteEvents() const;
        [[nodiscard]] services::ISavedViewRepository& SavedViews() const;
        [[nodiscard]] services::ISessionRepository& Sessions() const;

    private:
        core::ServiceProvider m_services;
    };
}
