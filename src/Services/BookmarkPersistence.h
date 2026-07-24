// Created by OpenAI Codex on Jul 24, 2026
#pragma once

#include "LocalDataFile.h"

#include <winrt/Windows.Storage.h>

namespace AstralChronicle::services::details
{
    inline constexpr wchar_t EventLogBookmarksLegacyStorageKey[] =
        L"EventLogs.BookmarkedRecordIds";
    inline constexpr wchar_t EventLogBookmarksStorageFileName[] =
        L"event-log-bookmarks.v2.txt";
    inline constexpr std::size_t MaximumEventLogBookmarksFileBytes =
        4u * 1024u * 1024u;

    [[nodiscard]] inline bool EventLogBookmarkPersistenceEnabled() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const key = winrt::hstring{ L"Storage.PersistBookmarks" };
            return !values.HasKey(key) || winrt::unbox_value<bool>(values.Lookup(key));
        }
        catch (...)
        {
            return true;
        }
    }

    [[nodiscard]] inline bool RemoveLegacyEventLogBookmarks() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            values.Remove(winrt::hstring{ EventLogBookmarksLegacyStorageKey });
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] inline bool ClearPersistedEventLogBookmarks() noexcept
    {
        LocalDataTransactionLock const transaction{
            EventLogBookmarksStorageFileName,
        };
        if (!transaction)
        {
            return false;
        }
        auto const fileRemoved =
            DeleteLocalDataFile(EventLogBookmarksStorageFileName);
        auto const legacyRemoved = RemoveLegacyEventLogBookmarks();
        return fileRemoved && legacyRemoved;
    }
}
