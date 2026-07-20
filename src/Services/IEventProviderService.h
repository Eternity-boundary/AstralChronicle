// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventQueryService.h"
#include "Models/EventProvider.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace AstralChronicle::services
{
    struct EventProviderQueryResult final
    {
        EventQueryStatus Status{ EventQueryStatus::Failed };
        std::uint32_t ErrorCode{};
        std::vector<models::EventProviderSummary> Providers;
    };

    struct EventProviderDetailsResult final
    {
        EventQueryStatus Status{ EventQueryStatus::Failed };
        std::uint32_t ErrorCode{};
        models::EventProviderDetails Details;
    };

    struct IEventProviderService
    {
        virtual ~IEventProviderService() = default;

        [[nodiscard]] virtual EventProviderQueryResult QueryProviders(
            std::wstring_view searchText,
            std::uint32_t maximumProviders,
            QueryCancellation const& cancellation) const = 0;

        [[nodiscard]] virtual EventProviderDetailsResult QueryDetails(
            std::wstring_view providerName,
            QueryCancellation const& cancellation) const = 0;
    };
}
