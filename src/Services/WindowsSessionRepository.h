// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "ISessionRepository.h"

#include <mutex>

namespace AstralChronicle::services
{
    class WindowsSessionRepository final : public ISessionRepository
    {
    public:
        [[nodiscard]] std::vector<models::DiagnosticSession> Load() const override;
        bool Upsert(models::DiagnosticSession const& session) override;
        bool Remove(std::wstring_view id) override;

    private:
        void EnsureLoaded() const;

        mutable std::mutex m_mutex;
        mutable std::vector<models::DiagnosticSession> m_sessions;
        mutable bool m_loaded{};
        mutable bool m_storageHealthy{ true };
        mutable bool m_persistenceDisabledApplied{};
        mutable bool m_persistenceDisabledObserved{};
    };
}
