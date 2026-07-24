// Created by OpenAI Codex on Jul 23, 2026
#pragma once

#include "LocalDataFile.h"

#include <winrt/Windows.Storage.h>

namespace AstralChronicle::services::details
{
    inline constexpr wchar_t DiagnosticSessionsLegacyStorageKey[] =
        L"DiagnosticSessions.Items";
    inline constexpr wchar_t DiagnosticSessionsStorageFileName[] =
        L"diagnostic-sessions.v1.tsv";

    [[nodiscard]] inline bool RemoveLegacyDiagnosticSessions() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            values.Remove(winrt::hstring{ DiagnosticSessionsLegacyStorageKey });
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] inline bool ClearPersistedDiagnosticSessions() noexcept
    {
        LocalDataTransactionLock const transaction{
            DiagnosticSessionsStorageFileName,
        };
        if (!transaction)
        {
            return false;
        }
        auto const fileRemoved =
            DeleteLocalDataFile(DiagnosticSessionsStorageFileName);
        auto const legacyRemoved = RemoveLegacyDiagnosticSessions();
        return fileRemoved && legacyRemoved;
    }
}
