// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "ISessionRepository.h"
namespace AstralChronicle::services
{
    class WindowsSessionRepository final : public ISessionRepository
    {
    public:
        [[nodiscard]] std::vector<models::DiagnosticSession> Load() const override;
        bool Upsert(models::DiagnosticSession const& session) override;
        bool Remove(std::wstring_view id) override;
    };
}
