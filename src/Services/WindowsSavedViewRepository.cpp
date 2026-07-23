// Created by EternityBoundary on Jul 20, 2026
#include "pch.h"
#include "WindowsSavedViewRepository.h"

#include "LocalDataFile.h"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace models = AstralChronicle::models;
    namespace storage = AstralChronicle::services::details;

    constexpr wchar_t LegacyStorageKey[] = L"SavedViews.Items";
    constexpr wchar_t StorageFileName[] = L"saved-views.v1.tsv";
    constexpr std::size_t MaximumStorageBytes = 16 * 1024 * 1024;

    struct SavedViewsLoadResult final
    {
        std::vector<models::SavedView> Items;
        bool CanPersist{ true };
    };

    struct SavedViewsParseResult final
    {
        std::vector<models::SavedView> Items;
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
        std::vector<std::wstring> values;
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
                values.emplace_back(Unescape(line.substr(start, index - start)));
                start = index + 1;
            }
        }
        return values;
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

    [[nodiscard]] std::optional<std::uint32_t> ParseUInt(
        std::wstring_view const value) noexcept
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        std::uint64_t parsed{};
        for (auto const character : value)
        {
            if (character < L'0' || character > L'9')
            {
                return std::nullopt;
            }
            parsed = parsed * 10 + static_cast<std::uint32_t>(character - L'0');
            if (parsed > (std::numeric_limits<std::uint32_t>::max)())
            {
                return std::nullopt;
            }
        }
        return static_cast<std::uint32_t>(parsed);
    }

    [[nodiscard]] std::wstring Serialize(
        std::vector<models::SavedView> const& views)
    {
        std::wstring result;
        for (auto const& view : views)
        {
            std::array<std::wstring, 15> const fields{
                view.Id,
                view.Name,
                view.Description,
                view.Query,
                view.Channel,
                std::to_wstring(static_cast<std::uint32_t>(view.Type)),
                view.Sort,
                view.Columns,
                view.Details ? L"1" : L"0",
                view.Timeline ? L"1" : L"0",
                view.Tags,
                view.CreatedAt,
                view.UpdatedAt,
                view.IsPinned ? L"1" : L"0",
                view.CustomViewXml,
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

    [[nodiscard]] SavedViewsParseResult Deserialize(std::wstring_view const text)
    {
        SavedViewsParseResult result;
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
            if (fields.size() != 15 || fields[0].empty())
            {
                result.Valid = false;
                result.Items.clear();
                return result;
            }

            auto const type = ParseUInt(fields[5]);
            auto const details = ParseBool(fields[8]);
            auto const timeline = ParseBool(fields[9]);
            auto const pinned = ParseBool(fields[13]);
            if (!type ||
                *type > static_cast<std::uint32_t>(models::SavedViewType::Workspace) ||
                !details ||
                !timeline ||
                !pinned)
            {
                result.Valid = false;
                result.Items.clear();
                return result;
            }

            models::SavedView view;
            view.Id = std::move(fields[0]);
            view.Name = std::move(fields[1]);
            view.Description = std::move(fields[2]);
            view.Query = std::move(fields[3]);
            view.Channel = std::move(fields[4]);
            view.Type = static_cast<models::SavedViewType>(*type);
            view.Sort = std::move(fields[6]);
            view.Columns = std::move(fields[7]);
            view.Details = *details;
            view.Timeline = *timeline;
            view.Tags = std::move(fields[10]);
            view.CreatedAt = std::move(fields[11]);
            view.UpdatedAt = std::move(fields[12]);
            view.IsPinned = *pinned;
            view.CustomViewXml = std::move(fields[14]);
            result.Items.emplace_back(std::move(view));
        }
        return result;
    }

    [[nodiscard]] storage::LocalTextReadResult ReadLegacyText() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const key = winrt::hstring{ LegacyStorageKey };
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

    void RemoveLegacyText() noexcept
    {
        try
        {
            auto const values =
                winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            values.Remove(winrt::hstring{ LegacyStorageKey });
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] bool Persist(std::vector<models::SavedView> const& views)
    {
        auto const serialized = Serialize(views);
        if (!storage::WriteLocalUtf8TextAtomically(
                StorageFileName,
                serialized,
                MaximumStorageBytes))
        {
            return false;
        }
        RemoveLegacyText();
        return true;
    }

    [[nodiscard]] SavedViewsLoadResult LoadUnlocked()
    {
        auto const file = storage::ReadLocalUtf8Text(
            StorageFileName,
            MaximumStorageBytes);
        if (file.Status == storage::LocalTextReadStatus::Succeeded)
        {
            auto parsed = Deserialize(file.Text);
            if (!parsed.Valid)
            {
                return { {}, false };
            }
            RemoveLegacyText();
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
            RemoveLegacyText();
        }
        return { std::move(parsed.Items), true };
    }
}

namespace AstralChronicle::services
{
    std::vector<models::SavedView> WindowsSavedViewRepository::Load() const
    {
        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{ StorageFileName };
        if (!transaction)
        {
            return {};
        }
        return LoadUnlocked().Items;
    }

    bool WindowsSavedViewRepository::Upsert(models::SavedView const& view)
    {
        if (view.Id.empty())
        {
            return false;
        }

        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{ StorageFileName };
        if (!transaction)
        {
            return false;
        }
        auto loaded = LoadUnlocked();
        if (!loaded.CanPersist)
        {
            return false;
        }

        auto existing = std::find_if(
            loaded.Items.begin(),
            loaded.Items.end(),
            [&view](auto const& candidate)
            {
                return candidate.Id == view.Id;
            });
        if (existing == loaded.Items.end())
        {
            loaded.Items.emplace_back(view);
        }
        else
        {
            *existing = view;
        }
        return Persist(loaded.Items);
    }

    bool WindowsSavedViewRepository::Remove(std::wstring_view const id)
    {
        std::scoped_lock lock{ m_mutex };
        storage::LocalDataTransactionLock const transaction{ StorageFileName };
        if (!transaction)
        {
            return false;
        }
        auto loaded = LoadUnlocked();
        if (!loaded.CanPersist)
        {
            return false;
        }

        auto const oldSize = loaded.Items.size();
        loaded.Items.erase(
            std::remove_if(
                loaded.Items.begin(),
                loaded.Items.end(),
                [id](auto const& view)
                {
                    return view.Id == id;
                }),
            loaded.Items.end());
        return oldSize != loaded.Items.size() && Persist(loaded.Items);
    }
}
