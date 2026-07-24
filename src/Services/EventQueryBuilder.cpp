// Created by EternityBoundary on Jul 20,2026
#include "EventQueryBuilder.h"

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <limits>
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

    constexpr std::wstring_view MatchNoneQuery = L"*[System[EventRecordID=0 and EventRecordID=1]]";

    [[nodiscard]] std::optional<std::wstring> NumericLiteral(
        std::wstring_view value,
        std::uint64_t const maximumValue)
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
        if (valueAsNumber > maximumValue)
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

    [[nodiscard]] std::optional<std::uint32_t> XmlEntityCodePoint(std::wstring_view entity)
    {
        if (entity == L"amp") return L'&';
        if (entity == L"lt") return L'<';
        if (entity == L"gt") return L'>';
        if (entity == L"quot") return L'"';
        if (entity == L"apos") return L'\'';
        if (entity.size() < 2 || entity.front() != L'#') return std::nullopt;

        std::size_t cursor{ 1 };
        std::uint32_t base{ 10 };
        if (cursor < entity.size() && (entity[cursor] == L'x' || entity[cursor] == L'X'))
        {
            base = 16;
            ++cursor;
        }
        if (cursor == entity.size()) return std::nullopt;

        std::uint32_t value{};
        for (; cursor < entity.size(); ++cursor)
        {
            auto const character = entity[cursor];
            std::uint32_t digit{};
            if (character >= L'0' && character <= L'9')
            {
                digit = static_cast<std::uint32_t>(character - L'0');
            }
            else if (base == 16 && character >= L'a' && character <= L'f')
            {
                digit = static_cast<std::uint32_t>(character - L'a' + 10);
            }
            else if (base == 16 && character >= L'A' && character <= L'F')
            {
                digit = static_cast<std::uint32_t>(character - L'A' + 10);
            }
            else
            {
                return std::nullopt;
            }
            if (digit >= base || value > (0x10FFFFu - digit) / base)
            {
                return std::nullopt;
            }
            value = value * base + digit;
        }
        if (value == 0 || value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
        {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<std::wstring> DecodeXmlText(std::wstring_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t cursor{}; cursor < value.size(); ++cursor)
        {
            if (value[cursor] != L'&')
            {
                result.push_back(value[cursor]);
                continue;
            }

            auto const terminator = value.find(L';', cursor + 1);
            if (terminator == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            auto const codePoint = XmlEntityCodePoint(value.substr(cursor + 1, terminator - cursor - 1));
            if (!codePoint)
            {
                return std::nullopt;
            }
            if (*codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<wchar_t>(*codePoint));
            }
            else
            {
                auto const supplementary = *codePoint - 0x10000u;
                result.push_back(static_cast<wchar_t>(0xD800u + (supplementary >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00u + (supplementary & 0x3FFu)));
            }
            cursor = terminator;
        }
        return result;
    }

    [[nodiscard]] std::wstring EncodeXmlText(std::wstring_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (auto const character : value)
        {
            switch (character)
            {
            case L'&': result += L"&amp;"; break;
            case L'<': result += L"&lt;"; break;
            case L'>': result += L"&gt;"; break;
            default: result.push_back(character); break;
            }
        }
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
        auto const addNumericPredicate = [&predicates](
            std::wstring_view const input,
            std::uint64_t const maximumValue,
            std::wstring_view const prefix,
            std::wstring_view const suffix = std::wstring_view{})
        {
            if (Trim(input).empty())
            {
                return true;
            }
            auto const value = NumericLiteral(input, maximumValue);
            if (!value)
            {
                return false;
            }
            predicates.emplace_back(
                std::wstring{ prefix } + *value + std::wstring{ suffix });
            return true;
        };

        if (!filter.Provider.empty())
        {
            predicates.emplace_back(L"Provider[@Name=" + XPathLiteral(filter.Provider) + L']');
        }
        if (!addNumericPredicate(
                filter.EventId,
                (std::numeric_limits<std::uint16_t>::max)(),
                L"EventID="))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.Level,
                (std::numeric_limits<std::uint8_t>::max)(),
                L"Level="))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.Task,
                (std::numeric_limits<std::uint16_t>::max)(),
                L"Task="))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.Opcode,
                (std::numeric_limits<std::uint8_t>::max)(),
                L"Opcode="))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.Keyword,
                (std::numeric_limits<std::uint64_t>::max)(),
                L"band(Keywords, ",
                L")"))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.ProcessId,
                (std::numeric_limits<std::uint32_t>::max)(),
                L"Execution[@ProcessID=",
                L"]"))
        {
            return std::wstring{ MatchNoneQuery };
        }
        if (!addNumericPredicate(
                filter.ThreadId,
                (std::numeric_limits<std::uint32_t>::max)(),
                L"Execution[@ThreadID=",
                L"]"))
        {
            return std::wstring{ MatchNoneQuery };
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
            auto const decodedQuery = DecodeXmlText(originalQuery);
            if (!decodedQuery)
            {
                return std::nullopt;
            }
            auto const combinedQuery = CombineEventQueries(*decodedQuery, additionalQuery);
            if (!combinedQuery)
            {
                return std::nullopt;
            }

            result.append(queryList.substr(cursor, contentStart + 1 - cursor));
            result.append(EncodeXmlText(*combinedQuery));
            cursor = selectEnd;
            applied = true;
        }

        return applied ? std::optional<std::wstring>{ std::move(result) } : std::nullopt;
    }
}
