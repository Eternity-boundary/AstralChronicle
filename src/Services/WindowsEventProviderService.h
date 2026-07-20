// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "IEventProviderService.h"

#include <mutex>
#include <unordered_map>

namespace AstralChronicle::services
{
    class WindowsEventProviderService final : public IEventProviderService
    {
    public:
        [[nodiscard]] EventProviderQueryResult QueryProviders(
            std::wstring_view searchText,
            std::uint32_t maximumProviders,
            QueryCancellation const& cancellation) const override;

        [[nodiscard]] EventProviderDetailsResult QueryDetails(
            std::wstring_view providerName,
            QueryCancellation const& cancellation) const override;

    private:
        mutable std::mutex m_cacheMutex;
        mutable std::unordered_map<std::wstring, models::EventProviderDetails> m_detailsCache;
    };
}
