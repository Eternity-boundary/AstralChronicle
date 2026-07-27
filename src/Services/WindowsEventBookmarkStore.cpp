// Created by EternityBoundary on Jul 28, 2026
#include "pch.h"
#include "WindowsEventBookmarkStore.h"

#include "BookmarkPersistence.h"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace
{
    constexpr std::size_t MaximumBookmarkCount = 100'000u;
    constexpr std::size_t MaximumBookmarkKeyCharacters = 256u * 1024u;

    [[nodiscard]] bool IsDecimal(
        std::wstring_view const value,
        bool const allowLeadingMinus = false) noexcept
    {
        if (value.empty()) return false;
        auto index = std::size_t{};
        if (allowLeadingMinus && value.front() == L'-')
        {
            if (value.size() == 1) return false;
            index = 1;
        }
        return std::all_of(value.begin() + index, value.end(), [](wchar_t const character)
            { return character >= L'0' && character <= L'9'; });
    }

    [[nodiscard]] bool IsHexEncoded(std::wstring_view const value) noexcept
    {
        return value.size() % 4 == 0 && std::all_of(value.begin(), value.end(), [](wchar_t const character)
            {
                return (character >= L'0' && character <= L'9') ||
                    (character >= L'A' && character <= L'F') ||
                    (character >= L'a' && character <= L'f');
            });
    }

    [[nodiscard]] bool IsValidBookmarkKey(std::wstring_view const value) noexcept
    {
        if (value.empty() || value.size() > MaximumBookmarkKeyCharacters ||
            value.find_first_of(L";\r\n\0", 0, 4) != std::wstring_view::npos)
        {
            return false;
        }
        if (!value.starts_with(L"v2|"))
        {
            auto const separator = value.rfind(L'|');
            return separator != std::wstring_view::npos && separator != 0 &&
                IsDecimal(value.substr(separator + 1));
        }

        std::array<std::wstring_view, 6> fields;
        std::size_t start{};
        for (std::size_t index{}; index < fields.size(); ++index)
        {
            auto const separator = value.find(L'|', start);
            if (index + 1 == fields.size())
            {
                if (separator != std::wstring_view::npos) return false;
                fields[index] = value.substr(start);
            }
            else
            {
                if (separator == std::wstring_view::npos) return false;
                fields[index] = value.substr(start, separator - start);
                start = separator + 1;
            }
        }
        return fields[0] == L"v2" && IsDecimal(fields[1]) && IsDecimal(fields[2], true) &&
            IsHexEncoded(fields[3]) && IsHexEncoded(fields[4]) && IsHexEncoded(fields[5]);
    }

    [[nodiscard]] bool ParseBookmarks(
        std::wstring_view const text,
        std::unordered_set<std::wstring>& result) noexcept
    {
        result.clear();
        if (text.size() > AstralChronicle::services::details::MaximumEventLogBookmarksFileBytes)
        {
            return false;
        }
        if (text.empty()) return true;

        try
        {
            std::size_t start{};
            while (start < text.size())
            {
                auto const separator = text.find(L';', start);
                auto const token = text.substr(start, separator == std::wstring_view::npos
                    ? std::wstring_view::npos
                    : separator - start);
                if (!IsValidBookmarkKey(token) || result.size() >= MaximumBookmarkCount)
                {
                    result.clear();
                    return false;
                }
                result.emplace(token);
                if (separator == std::wstring_view::npos) return true;
                start = separator + 1;
            }
        }
        catch (...)
        {
            result.clear();
            return false;
        }
        result.clear();
        return false;
    }

    [[nodiscard]] std::wstring SerializeBookmarks(
        std::unordered_set<std::wstring> const& bookmarks)
    {
        std::wstring text;
        for (auto const& bookmark : bookmarks)
        {
            if (!text.empty()) text += L';';
            text += bookmark;
        }
        return text;
    }

    void RemoveLegacyBookmarks() noexcept
    {
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values();
            values.Remove(winrt::hstring{
                AstralChronicle::services::details::EventLogBookmarksLegacyStorageKey });
        }
        catch (...)
        {
        }
    }
}

namespace AstralChronicle::services
{
    std::unordered_set<std::wstring> WindowsEventBookmarkStore::Load() const
    {
        if (!details::EventLogBookmarkPersistenceEnabled())
        {
            Clear();
            return {};
        }

        details::LocalDataTransactionLock const transaction{
            details::EventLogBookmarksStorageFileName };
        if (!transaction) return {};

        auto const file = details::ReadLocalUtf8Text(
            details::EventLogBookmarksStorageFileName,
            details::MaximumEventLogBookmarksFileBytes);
        std::unordered_set<std::wstring> parsed;
        if (file.Status == details::LocalTextReadStatus::Succeeded &&
            ParseBookmarks(file.Text, parsed))
        {
            RemoveLegacyBookmarks();
            return parsed;
        }
        if (file.Status == details::LocalTextReadStatus::Failed)
        {
            return {};
        }

        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values();
            auto const legacyKey = winrt::hstring{ details::EventLogBookmarksLegacyStorageKey };
            if (!values.HasKey(legacyKey)) return {};
            auto const stored = winrt::unbox_value<winrt::hstring>(values.Lookup(legacyKey));
            if (!ParseBookmarks(std::wstring_view{ stored.c_str(), stored.size() }, parsed))
            {
                return {};
            }
            if (details::WriteLocalUtf8TextAtomically(
                    details::EventLogBookmarksStorageFileName,
                    SerializeBookmarks(parsed),
                    details::MaximumEventLogBookmarksFileBytes))
            {
                values.Remove(legacyKey);
            }
            return parsed;
        }
        catch (...)
        {
            return {};
        }
    }

    void WindowsEventBookmarkStore::Save(
        std::unordered_set<std::wstring> const& bookmarks) const noexcept
    {
        if (!details::EventLogBookmarkPersistenceEnabled())
        {
            Clear();
            return;
        }

        try
        {
            details::LocalDataTransactionLock const transaction{
                details::EventLogBookmarksStorageFileName };
            if (transaction)
            {
                (void)details::WriteLocalUtf8TextAtomically(
                    details::EventLogBookmarksStorageFileName,
                    SerializeBookmarks(bookmarks),
                    details::MaximumEventLogBookmarksFileBytes);
            }
        }
        catch (...)
        {
        }
    }

    void WindowsEventBookmarkStore::Clear() const noexcept
    {
        (void)details::ClearPersistedEventLogBookmarks();
    }
}
