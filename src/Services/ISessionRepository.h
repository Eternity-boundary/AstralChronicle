// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "Models/DiagnosticSession.h"
#include <string_view>
#include <vector>
namespace AstralChronicle::services
{
    struct ISessionRepository
    {
        virtual ~ISessionRepository() = default;
        [[nodiscard]] virtual std::vector<models::DiagnosticSession> Load() const = 0;
        virtual bool Upsert(models::DiagnosticSession const& session) = 0;
        virtual bool Remove(std::wstring_view id) = 0;
    };
}
