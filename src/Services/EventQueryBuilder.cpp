// Created by EternityBoundary on Jul 20,2026
#include "EventQueryBuilder.h"

#include <algorithm>
#include <cwctype>
#include <string_view>
#include <vector>

namespace
{
    [[nodiscard]] bool IsDigits(std::wstring_view value)
    {
        return !value.empty() && std::all_of(
            value.begin(),
            value.end(),
            [](wchar_t const character)
            {
                return std::iswdigit(character) != 0;
            });
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
        if (IsDigits(filter.EventId))
        {
            predicates.emplace_back(L"EventID=" + filter.EventId);
        }
        if (IsDigits(filter.Level))
        {
            predicates.emplace_back(L"Level=" + filter.Level);
        }
        if (!filter.Task.empty() && IsDigits(filter.Task))
        {
            predicates.emplace_back(L"Task=" + filter.Task);
        }
        if (!filter.Opcode.empty() && IsDigits(filter.Opcode))
        {
            predicates.emplace_back(L"Opcode=" + filter.Opcode);
        }
        if (!filter.Keyword.empty() && IsDigits(filter.Keyword))
        {
            predicates.emplace_back(L"Keywords=" + filter.Keyword);
        }
        if (!filter.ProcessId.empty() && IsDigits(filter.ProcessId))
        {
            predicates.emplace_back(L"Execution[@ProcessID=" + filter.ProcessId + L"]");
        }
        if (!filter.ThreadId.empty() && IsDigits(filter.ThreadId))
        {
            predicates.emplace_back(L"Execution[@ThreadID=" + filter.ThreadId + L"]");
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
}
