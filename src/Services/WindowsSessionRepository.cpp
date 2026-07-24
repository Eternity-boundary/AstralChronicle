// Created by EternityBoundary on Jul 20, 2026
#include "pch.h"
#include "WindowsSessionRepository.h"

#include "LocalDataFile.h"
#include "SessionPersistence.h"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace models = AstralChronicle::models;
    namespace storage = AstralChronicle::services::details;

    constexpr wchar_t PersistSessionsSettingKey[] = L"Storage.PersistSessions";
    constexpr std::size_t MaximumStorageBytes = 16 * 1024 * 1024;

    struct SessionsLoadResult final
    {
        std::vector<models::DiagnosticSession> Items;
        bool CanPersist{ true };
    };

    struct SessionsParseResult final
    {
        std::vector<models::DiagnosticSession> Items;
        bool Valid{ true };
    };

    [[nodiscard]] std::wstring Escape(std::wstring_view const value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (auto const character : value)
        {
            switch (character)
            {
            case L'\\': result += L"\\\\"; break;
            case L'\t': result += L"\\t"; break;
            case L'\r': result += L"\\r"; break;
            case L'\n': result += L"\\n"; break;
            default: result += character; break;
            }
        }
        return result;
    }

    [[nodiscard]] std::wstring Unescape(std::wstring_view const value)
    {
        std::wstring result;
        bool escaped{};
        for (auto const character : value)
        {
            if (!escaped && character == L'\\')
            {
                escaped = true;
                continue;
            }
            if (escaped)
            {
                switch (character)
                {
                case L't': result += L'\t'; break;
                case L'r': result += L'\r'; break;
                case L'n': result += L'\n'; break;
                default: result += character; break;
                }
                escaped = false;
                continue;
            }
            result += character;
        }
        if (escaped)
        {
            result += L'\\';
        }
        return result;
    }

    [[nodiscard]] std::vector<std::wstring> Split(std::wstring_view const line)
    {
        std::vector<std::wstring> result;
        std::size_t start{};
        bool escaped{};
        for (std::size_t index{}; index <= line.size(); ++index)
        {
            if (index < line.size() && !escaped && line[index] == L'\\')
            {
                escaped = true;
                continue;
            }
            if (index < line.size() && escaped)
            {
                escaped = false;
                continue;
            }
            if (index == line.size() || line[index] == L'\t')
            {
                result.emplace_back(Unescape(line.substr(start, index - start)));
                start = index + 1;
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<bool> ParseBool(std::wstring_view const value) noexcept
    {
        if (value == L"1" || value == L"true")
        {
            return true;
        }
        if (value == L"0" || value == L"false")
        {
            return false;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::wstring Serialize(
        std::vector<models::DiagnosticSession> const& sessions)
    {
        std::wstring result;
        for (auto const& session : sessions)
        {
            std::array<std::wstring, 18> const fields{
                session.Id,
                session.Name,
                session.Description,
                session.Channels,
                session.Filter,
                session.Tags,
                session.Notes,
                session.CorrelationGroup,
                session.RemoteTarget,
                session.CreatedAt,
                session.LastResumedAt,
                session.IsPinned ? L"1" : L"0",
                session.IsActive ? L"1" : L"0",
                session.StartAt,
                session.EndAt,
                session.BookmarkedEventIds,
                session.ExportSettings,
                session.IsArchived ? L"1" : L"0",
            };
            for (std::size_t index{}; index < fields.size(); ++index)
            {
                if (index != 0)
                {
                    result += L'\t';
                }
                result += Escape(fields[index]);
            }
            result += L'\n';
        }
        return result;
    }

    [[nodiscard]] SessionsParseResult Deserialize(std::wstring_view const text)
    {
        SessionsParseResult result;
        std::size_t start{};
        for (std::size_t index{}; index <= text.size(); ++index)
        {
            if (index != text.size() && text[index] != L'\n')
            {
                continue;
            }

            auto const line = text.substr(start, index - start);
            start = index + 1;
            if (line.empty())
            {
                continue;
            }

            auto fields = Split(line);
            if (fields.size() < 13 || fields.size() > 18 || fields[0].empty())
            {
                result.Valid = false;
                result.Items.clear();
                return result;
            }

            auto const pinned = ParseBool(fields[11]);
            auto const active = ParseBool(fields[12]);
            auto const archived = fields.size() > 17
                ? ParseBool(fields[17])
                : std::optional<bool>{ false };
            if (!pinned || !active || !archived)
            {
                result.Valid = false;
                result.Items.clear();
                return result;
            }

            models::DiagnosticSession session;
            session.Id = std::move(fields[0]);
            session.Name = std::move(fields[1]);
            session.Description = std::move(fields[2]);
            session.Channels = std::move(fields[3]);
            session.Filter = std::move(fields[4]);
            session.Tags = std::move(fields[5]);
            session.Notes = std::move(fields[6]);
            session.CorrelationGroup = std::move(fields[7]);
            session.RemoteTarget = std::move(fields[8]);
            session.CreatedAt = std::move(fields[9]);
            session.LastResumedAt = std::move(fields[10]);
            session.IsPinned = *pinned;
            session.IsActive = *active;
            if (fields.size() > 13) session.StartAt = std::move(fields[13]);
            if (fields.size() > 14) session.EndAt = std::move(fields[14]);
            if (fields.size() > 15) session.BookmarkedEventIds = std::move(fields[15]);
            if (fields.size() > 16) session.ExportSettings = std::move(fields[16]);
            session.IsArchived = *archived;
            result.Items.emplace_back(std::move(session));
        }
        return result;
    }

    [[nodiscard]] storage::LocalTextReadResult ReadLegacyText() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const key =
                winrt::hstring{ storage::DiagnosticSessionsLegacyStorageKey };
            if (!values.HasKey(key))
            {
                return { storage::LocalTextReadStatus::Missing, {} };
            }
            auto const text = winrt::unbox_value<winrt::hstring>(values.Lookup(key));
            return {
                storage::LocalTextReadStatus::Succeeded,
                std::wstring{ text.c_str(), text.size() },
            };
        }
        catch (...)
        {
            return {};
        }
    }

    [[nodiscard]] bool PersistenceEnabled() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const key = winrt::hstring{ PersistSessionsSettingKey };
            return !values.HasKey(key) || winrt::unbox_value<bool>(values.Lookup(key));
        }
        catch (...)
        {
            return true;
        }
    }

    [[nodiscard]] bool Persist(
        std::vector<models::DiagnosticSession> const& sessions)
    {
        auto const serialized = Serialize(sessions);
        if (!storage::WriteLocalUtf8TextAtomically(
                storage::DiagnosticSessionsStorageFileName,
                serialized,
                MaximumStorageBytes))
        {
            return false;
        }
        (void)storage::RemoveLegacyDiagnosticSessions();
        return true;
    }

    [[nodiscard]] SessionsLoadResult LoadFromStorage()
    {
        auto const file = storage::ReadLocalUtf8Text(
            storage::DiagnosticSessionsStorageFileName,
            MaximumStorageBytes);
        if (file.Status == storage::LocalTextReadStatus::Succeeded)
        {
            auto parsed = Deserialize(file.Text);
            if (!parsed.Valid)
            {
                return { {}, false };
            }
            (void)storage::RemoveLegacyDiagnosticSessions();
            return { std::move(parsed.Items), true };
        }
        if (file.Status == storage::LocalTextReadStatus::Failed)
        {
            return { {}, false };
        }

        auto const legacy = ReadLegacyText();
        if (legacy.Status == storage::LocalTextReadStatus::Missing)
        {
            return {};
        }
        if (legacy.Status == storage::LocalTextReadStatus::Failed)
        {
            return { {}, false };
        }

        auto parsed = Deserialize(legacy.Text);
        if (!parsed.Valid)
        {
            return { {}, false };
        }
        if (Persist(parsed.Items))
        {
            (void)storage::RemoveLegacyDiagnosticSessions();
        }
        return { std::move(parsed.Items), true };
    }
}

namespace AstralChronicle::services
{
    void WindowsSessionRepository::EnsureLoaded() const
    {
        auto const persistenceEnabled = PersistenceEnabled();
        if (!persistenceEnabled)
        {
            if (!m_loaded)
            {
                m_sessions.clear();
                m_loaded = true;
            }
            m_persistenceDisabledObserved = true;
            if (!m_persistenceDisabledApplied)
            {
                m_persistenceDisabledApplied =
                    storage::ClearPersistedDiagnosticSessions();
                if (m_persistenceDisabledApplied)
                {
                    m_storageHealthy = true;
                }
            }
            return;
        }

        if (m_persistenceDisabledObserved)
        {
            m_storageHealthy = m_persistenceDisabledApplied && Persist(m_sessions);
            if (m_storageHealthy)
            {
                m_persistenceDisabledApplied = false;
                m_persistenceDisabledObserved = false;
            }
            m_loaded = true;
            return;
        }

        auto loaded = LoadFromStorage();
        if (loaded.CanPersist)
        {
            m_sessions = std::move(loaded.Items);
        }
        m_storageHealthy = loaded.CanPersist;
        m_persistenceDisabledApplied = false;
        m_persistenceDisabledObserved = false;
        m_loaded = true;
    }

    std::vector<models::DiagnosticSession> WindowsSessionRepository::Load() const
    {
        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{
            storage::DiagnosticSessionsStorageFileName,
        };
        if (!transaction)
        {
            return {};
        }
        EnsureLoaded();
        return m_sessions;
    }

    bool WindowsSessionRepository::Upsert(models::DiagnosticSession const& session)
    {
        if (session.Id.empty())
        {
            return false;
        }

        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{
            storage::DiagnosticSessionsStorageFileName,
        };
        if (!transaction)
        {
            return false;
        }
        EnsureLoaded();
        auto staged = m_sessions;
        auto existing = std::find_if(
            staged.begin(),
            staged.end(),
            [&session](auto const& candidate)
            {
                return candidate.Id == session.Id;
            });
        if (existing == staged.end())
        {
            staged.emplace_back(session);
        }
        else
        {
            *existing = session;
        }

        if (PersistenceEnabled())
        {
            if (!m_storageHealthy || !Persist(staged))
            {
                return false;
            }
        }
        m_sessions = std::move(staged);
        return true;
    }

    bool WindowsSessionRepository::Remove(std::wstring_view const id)
    {
        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{
            storage::DiagnosticSessionsStorageFileName,
        };
        if (!transaction)
        {
            return false;
        }
        EnsureLoaded();
        auto staged = m_sessions;
        auto const oldSize = staged.size();
        staged.erase(
            std::remove_if(
                staged.begin(),
                staged.end(),
                [id](auto const& session)
                {
                    return session.Id == id;
                }),
            staged.end());
        if (oldSize == staged.size())
        {
            return false;
        }

        if (PersistenceEnabled())
        {
            if (!m_storageHealthy || !Persist(staged))
            {
                return false;
            }
        }
        m_sessions = std::move(staged);
        return true;
    }
}
