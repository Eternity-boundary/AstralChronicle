// Created by EternityBoundary on Jul 20,2026
#pragma once
#include <string>

namespace AstralChronicle::models
{
    struct DiagnosticSession final
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring Description;
        std::wstring Channels;
        std::wstring Filter{ L"*" };
        std::wstring Tags;
        std::wstring Notes;
        std::wstring CorrelationGroup;
        std::wstring RemoteTarget;
        std::wstring CreatedAt;
        std::wstring LastResumedAt;
        std::wstring StartAt;
        std::wstring EndAt;
        std::wstring BookmarkedEventIds;
        std::wstring ExportSettings;
        bool IsPinned{};
        bool IsActive{};
        bool IsArchived{};
    };
}
