// Created by EternityBoundary on Jul 20,2026
#include "EventQueryBuilder.h"

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    [[nodiscard]] std::wstring_view Trim(std::wstring_view value)
    {
        auto const start = value.find_first_not_of(L" \t\r\n");
        if (start == std::wstring_view::npos)
        {
            return {};
        }

        auto const end = value.find_last_not_of(L" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    [[nodiscard]] std::optional<std::wstring> NumericLiteral(std::wstring_view value)
    {
        auto const trimmed = Trim(value);
        if (trimmed.empty() || trimmed.front() == L'-')
        {
            return std::nullopt;
        }

        auto digits = trimmed;
        auto base = 10;
        if (digits.size() > 2 && digits[0] == L'0' &&
            (digits[1] == L'x' || digits[1] == L'X'))
        {
            digits.remove_prefix(2);
            base = 16;
        }

        if (digits.empty())
        {
            return std::nullopt;
        }

        std::wstring const numberText{ digits };
        wchar_t* end{};
        errno = 0;
        auto const valueAsNumber = std::wcstoull(numberText.c_str(), &end, base);
        if (errno == ERANGE || end != numberText.c_str() + numberText.size())
        {
            return std::nullopt;
        }

        return std::to_wstring(valueAsNumber);
    }

    [[nodiscard]] std::optional<std::wstring_view> EventQueryPredicate(std::wstring_view query)
    {
        auto const trimmed = Trim(query);
        if (trimmed == L"*")
        {
            return std::wstring_view{};
        }

        if (trimmed.size() < 4 || !trimmed.starts_with(L"*[") || trimmed.back() != L']')
        {
            return std::nullopt;
        }

        return trimmed.substr(2, trimmed.size() - 3);
    }

    [[nodiscard]] std::wstring XPathLiteral(std::wstring_view value)
    {
        if (value.find(L'\'') == std::wstring_view::npos)
        {
            return L"'" + std::wstring{ value } + L"'";
        }

        std::wstring result{ L"concat(" };
        std::size_t segmentStart{};
        bool firstSegment = true;
        while (segmentStart <= value.size())
        {
            auto const separator = value.find(L'\'', segmentStart);
            auto const segment = value.substr(
                segmentStart,
                separator == std::wstring_view::npos
                    ? std::wstring_view::npos
                    : separator - segmentStart);
            if (!firstSegment)
            {
                result += L", \"'\", ";
            }
            result += L"'" + std::wstring{ segment } + L"'";
            firstSegment = false;
            if (separator == std::wstring_view::npos)
            {
                break;
            }
            segmentStart = separator + 1;
        }
        result += L')';
        return result;
    }

}

namespace AstralChronicle::services
{
    std::wstring BuildEventQuery(models::EventFilter const& filter)
    {
        if (!filter.RawXPath.empty())
        {
            return filter.RawXPath;
        }

        std::vector<std::wstring> predicates;
        if (!filter.Provider.empty())
        {
            predicates.emplace_back(L"Provider[@Name=" + XPathLiteral(filter.Provider) + L']');
        }
        if (auto const eventId = NumericLiteral(filter.EventId))
        {
            predicates.emplace_back(L"EventID=" + *eventId);
        }
        if (auto const level = NumericLiteral(filter.Level))
        {
            predicates.emplace_back(L"Level=" + *level);
        }
        if (auto const task = NumericLiteral(filter.Task))
        {
            predicates.emplace_back(L"Task=" + *task);
        }
        if (auto const opcode = NumericLiteral(filter.Opcode))
        {
            predicates.emplace_back(L"Opcode=" + *opcode);
        }
        if (auto const keyword = NumericLiteral(filter.Keyword))
        {
            predicates.emplace_back(L"band(Keywords, " + *keyword + L")");
        }
        if (auto const processId = NumericLiteral(filter.ProcessId))
        {
            predicates.emplace_back(L"Execution[@ProcessID=" + *processId + L"]");
        }
        if (auto const threadId = NumericLiteral(filter.ThreadId))
        {
            predicates.emplace_back(L"Execution[@ThreadID=" + *threadId + L"]");
        }
        if (!filter.User.empty())
        {
            predicates.emplace_back(L"Security[@UserID=" + XPathLiteral(filter.User) + L']');
        }
        if (!filter.Computer.empty())
        {
            predicates.emplace_back(L"Computer=" + XPathLiteral(filter.Computer));
        }
        if (filter.AfterToday)
        {
            predicates.emplace_back(L"TimeCreated[timediff(@SystemTime) <= 86400000]");
        }
        else if (filter.AfterHours != 0)
        {
            auto const milliseconds = static_cast<std::uint64_t>(filter.AfterHours) * 3'600'000ULL;
            predicates.emplace_back(L"TimeCreated[timediff(@SystemTime) <= " + std::to_wstring(milliseconds) + L"]");
        }

        if (predicates.empty())
        {
            return L"*";
        }

        std::wstring result{ L"*[System[" };
        for (std::size_t index{}; index < predicates.size(); ++index)
        {
            if (index != 0)
            {
                result += L" and ";
            }
            result += predicates[index];
        }
        result += L"]]";
        return result;
    }

    std::optional<std::wstring> CombineEventQueries(
        std::wstring_view baseQuery,
        std::wstring_view additionalQuery)
    {
        auto const basePredicate = EventQueryPredicate(baseQuery);
        auto const additionalPredicate = EventQueryPredicate(additionalQuery);
        if (!basePredicate || !additionalPredicate)
        {
            return std::nullopt;
        }

        if (basePredicate->empty())
        {
            return std::wstring{ Trim(additionalQuery) };
        }
        if (additionalPredicate->empty())
        {
            return std::wstring{ Trim(baseQuery) };
        }

        return L"*[(" + std::wstring{ *basePredicate } + L") and (" +
            std::wstring{ *additionalPredicate } + L")]";
    }

    std::optional<std::wstring> ApplyFilterToStructuredQuery(
        std::wstring_view queryList,
        std::wstring_view additionalQuery)
    {
        std::wstring result;
        std::size_t cursor{};
        bool applied{};

        while (true)
        {
            auto const selectStart = queryList.find(L"<Select", cursor);
            if (selectStart == std::wstring_view::npos)
            {
                result.append(queryList.substr(cursor));
                break;
            }

            auto const contentStart = queryList.find(L'>', selectStart);
            if (contentStart == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            auto const selectEnd = queryList.find(L"</Select>", contentStart + 1);
            if (selectEnd == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            auto const originalQuery = queryList.substr(
                contentStart + 1,
                selectEnd - contentStart - 1);
            auto const combinedQuery = CombineEventQueries(originalQuery, additionalQuery);
            if (!combinedQuery)
            {
                return std::nullopt;
            }

            result.append(queryList.substr(cursor, contentStart + 1 - cursor));
            result.append(*combinedQuery);
            cursor = selectEnd;
            applied = true;
        }

        return applied ? std::optional<std::wstring>{ std::move(result) } : std::nullopt;
    }
}
