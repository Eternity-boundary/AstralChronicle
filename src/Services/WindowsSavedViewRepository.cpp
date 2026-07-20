// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsSavedViewRepository.h"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    namespace models = AstralChronicle::models;
    constexpr wchar_t StorageKey[] = L"SavedViews.Items";
    std::mutex RepositoryMutex;

    [[nodiscard]] std::wstring Escape(std::wstring_view value)
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

    [[nodiscard]] std::wstring Unescape(std::wstring_view value)
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
        if (escaped) result += L'\\';
        return result;
    }

    [[nodiscard]] std::vector<std::wstring> Split(std::wstring_view line)
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

    [[nodiscard]] std::wstring BoolText(bool const value) { return value ? L"1" : L"0"; }

    [[nodiscard]] bool ParseBool(std::wstring_view value)
    {
        return value == L"1" || value == L"true";
    }

    [[nodiscard]] std::uint32_t ParseUInt(std::wstring_view value)
    {
        try
        {
            return static_cast<std::uint32_t>(std::stoul(std::wstring{ value }));
        }
        catch (...)
        {
            return 0;
        }
    }

    [[nodiscard]] std::wstring Serialize(std::vector<models::SavedView> const& views)
    {
        std::wstring result;
        for (auto const& view : views)
        {
            std::array<std::wstring, 15> fields{
                view.Id, view.Name, view.Description, view.Query, view.Channel,
                std::to_wstring(static_cast<std::uint32_t>(view.Type)), view.Sort, view.Columns,
                BoolText(view.Details), BoolText(view.Timeline), view.Tags, view.CreatedAt,
                view.UpdatedAt, BoolText(view.IsPinned), view.CustomViewXml };
            for (std::size_t index{}; index < fields.size(); ++index)
            {
                if (index != 0) result += L'\t';
                result += Escape(fields[index]);
            }
            result += L'\n';
        }
        return result;
    }

    [[nodiscard]] std::vector<models::SavedView> Deserialize(std::wstring_view text);

    [[nodiscard]] std::vector<models::SavedView> LoadUnlocked()
    {
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const key = winrt::hstring{ StorageKey };
            if (!values.HasKey(key)) return {};
            auto const text = winrt::unbox_value<winrt::hstring>(values.Lookup(key));
            return Deserialize(std::wstring_view{ text.c_str(), text.size() });
        }
        catch (...)
        {
            return {};
        }
    }

    [[nodiscard]] std::vector<models::SavedView> Deserialize(std::wstring_view text)
    {
        std::vector<models::SavedView> views;
        std::size_t start{};
        for (std::size_t index{}; index <= text.size(); ++index)
        {
            if (index != text.size() && text[index] != L'\n') continue;
            auto fields = Split(text.substr(start, index - start));
            start = index + 1;
            if (fields.size() < 15 || fields[0].empty()) continue;
            models::SavedView view;
            view.Id = std::move(fields[0]);
            view.Name = std::move(fields[1]);
            view.Description = std::move(fields[2]);
            view.Query = std::move(fields[3]);
            view.Channel = std::move(fields[4]);
            view.Type = static_cast<models::SavedViewType>(ParseUInt(fields[5]));
            view.Sort = std::move(fields[6]);
            view.Columns = std::move(fields[7]);
            view.Details = ParseBool(fields[8]);
            view.Timeline = ParseBool(fields[9]);
            view.Tags = std::move(fields[10]);
            view.CreatedAt = std::move(fields[11]);
            view.UpdatedAt = std::move(fields[12]);
            view.IsPinned = ParseBool(fields[13]);
            view.CustomViewXml = std::move(fields[14]);
            views.emplace_back(std::move(view));
        }
        return views;
    }

    bool Persist(std::vector<models::SavedView> const& views)
    {
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            values.Insert(winrt::hstring{ StorageKey }, winrt::box_value(winrt::hstring{ Serialize(views) }));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

namespace AstralChronicle::services
{
    std::vector<models::SavedView> WindowsSavedViewRepository::Load() const
    {
        std::scoped_lock lock{ RepositoryMutex };
        return LoadUnlocked();
    }

    bool WindowsSavedViewRepository::Upsert(models::SavedView const& view)
    {
        if (view.Id.empty()) return false;
        std::scoped_lock lock{ RepositoryMutex };
        auto views = LoadUnlocked();
        auto existing = std::find_if(views.begin(), views.end(), [&view](auto const& candidate)
        {
            return candidate.Id == view.Id;
        });
        if (existing == views.end()) views.emplace_back(view);
        else *existing = view;
        return Persist(views);
    }

    bool WindowsSavedViewRepository::Remove(std::wstring_view id)
    {
        std::scoped_lock lock{ RepositoryMutex };
        auto views = LoadUnlocked();
        auto const oldSize = views.size();
        views.erase(std::remove_if(views.begin(), views.end(), [id](auto const& view)
        {
            return view.Id == id;
        }), views.end());
        return oldSize != views.size() && Persist(views);
    }
}
