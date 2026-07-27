// Created by EternityBoundary on Jul 27,2026
#include "pch.h"
#include "WindowsEventLiveDataService.h"

#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace AstralChronicle::services
{
    namespace
    {
        [[nodiscard]] std::optional<std::wstring> XmlAttribute(
            std::wstring_view const tag,
            std::wstring_view const attributeName)
        {
            auto position = tag.find(attributeName);
            while (position != std::wstring_view::npos)
            {
                auto const before = position == 0 ? L' ' : tag[position - 1];
                auto cursor = position + attributeName.size();
                if ((std::iswspace(before) || before == L'<') &&
                    (cursor >= tag.size() || std::iswspace(tag[cursor]) || tag[cursor] == L'='))
                {
                    while (cursor < tag.size() && std::iswspace(tag[cursor])) ++cursor;
                    if (cursor < tag.size() && tag[cursor] == L'=')
                    {
                        ++cursor;
                        while (cursor < tag.size() && std::iswspace(tag[cursor])) ++cursor;
                        if (cursor < tag.size() && (tag[cursor] == L'"' || tag[cursor] == L'\''))
                        {
                            auto const quote = tag[cursor++];
                            auto const end = tag.find(quote, cursor);
                            if (end != std::wstring_view::npos)
                            {
                                return std::wstring{ tag.substr(cursor, end - cursor) };
                            }
                        }
                    }
                }
                position = tag.find(attributeName, position + attributeName.size());
            }
            return std::nullopt;
        }

        [[nodiscard]] std::size_t FindOpeningTag(
            std::wstring_view const xml,
            std::wstring_view const elementName,
            std::size_t searchFrom = 0) noexcept
        {
            auto const prefix = std::wstring{ L"<" } + std::wstring{ elementName };
            auto position = xml.find(prefix, searchFrom);
            while (position != std::wstring_view::npos)
            {
                auto const boundary = position + prefix.size();
                if (boundary < xml.size() &&
                    (xml[boundary] == L'>' || xml[boundary] == L'/' || std::iswspace(xml[boundary])))
                {
                    return position;
                }
                position = xml.find(prefix, boundary);
            }
            return std::wstring_view::npos;
        }

        [[nodiscard]] std::optional<std::wstring_view> OpeningTag(
            std::wstring_view const xml,
            std::wstring_view const elementName,
            std::size_t searchFrom = 0) noexcept
        {
            auto const start = FindOpeningTag(xml, elementName, searchFrom);
            if (start == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            auto const end = xml.find(L'>', start);
            if (end == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            return xml.substr(start, end - start + 1);
        }

        [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> ElementContents(
            std::wstring_view const xml,
            std::wstring_view const elementName,
            std::size_t searchFrom = 0) noexcept
        {
            auto const start = FindOpeningTag(xml, elementName, searchFrom);
            if (start == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            auto const openingEnd = xml.find(L'>', start);
            if (openingEnd == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            if (openingEnd > start && xml[openingEnd - 1] == L'/')
            {
                return std::pair{ openingEnd + 1, openingEnd + 1 };
            }
            auto const closingTag = std::wstring{ L"</" } + std::wstring{ elementName } + L">";
            auto const closingStart = xml.find(closingTag, openingEnd + 1);
            if (closingStart == std::wstring_view::npos)
            {
                return std::nullopt;
            }
            return std::pair{ openingEnd + 1, closingStart };
        }

        [[nodiscard]] std::wstring_view TrimWhitespace(std::wstring_view value) noexcept
        {
            while (!value.empty() && std::iswspace(value.front())) value.remove_prefix(1);
            while (!value.empty() && std::iswspace(value.back())) value.remove_suffix(1);
            return value;
        }

        void ReplaceAll(
            std::wstring& value,
            std::wstring_view const needle,
            std::wstring_view const replacement)
        {
            std::size_t position{};
            while ((position = value.find(needle, position)) != std::wstring::npos)
            {
                value.replace(position, needle.size(), replacement);
                position += replacement.size();
            }
        }

        [[nodiscard]] std::wstring XmlUnescape(std::wstring_view value)
        {
            auto result = std::wstring{ value };
            ReplaceAll(result, L"&lt;", L"<");
            ReplaceAll(result, L"&gt;", L">");
            ReplaceAll(result, L"&quot;", L"\"");
            ReplaceAll(result, L"&apos;", L"'");
            ReplaceAll(result, L"&amp;", L"&");
            return result;
        }

        [[nodiscard]] std::wstring ElementText(
            std::wstring_view const xml,
            std::wstring_view const elementName)
        {
            auto const contents = ElementContents(xml, elementName);
            if (!contents)
            {
                return {};
            }
            return XmlUnescape(TrimWhitespace(xml.substr(
                contents->first,
                contents->second - contents->first)));
        }

        [[nodiscard]] std::optional<std::uint64_t> ParseUnsigned(std::wstring_view value) noexcept
        {
            value = TrimWhitespace(value);
            if (value.empty())
            {
                return std::nullopt;
            }

            auto base = 10u;
            if (value.size() > 2 && value[0] == L'0' && (value[1] == L'x' || value[1] == L'X'))
            {
                base = 16u;
                value.remove_prefix(2);
            }
            if (value.empty())
            {
                return std::nullopt;
            }

            std::uint64_t result{};
            for (auto const character : value)
            {
                std::uint32_t digit{};
                if (character >= L'0' && character <= L'9')
                {
                    digit = static_cast<std::uint32_t>(character - L'0');
                }
                else if (base == 16u && character >= L'a' && character <= L'f')
                {
                    digit = static_cast<std::uint32_t>(character - L'a' + 10);
                }
                else if (base == 16u && character >= L'A' && character <= L'F')
                {
                    digit = static_cast<std::uint32_t>(character - L'A' + 10);
                }
                else
                {
                    return std::nullopt;
                }
                if (digit >= base || result > ((std::numeric_limits<std::uint64_t>::max)() - digit) / base)
                {
                    return std::nullopt;
                }
                result = result * base + digit;
            }
            return result;
        }

        [[nodiscard]] std::chrono::system_clock::time_point ParseEventTime(std::wstring_view value) noexcept
        {
            value = TrimWhitespace(value);
            if (value.size() < 19 || value[4] != L'-' || value[7] != L'-' ||
                value[10] != L'T' || value[13] != L':' || value[16] != L':')
            {
                return {};
            }

            auto readNumber = [&value](std::size_t const offset, std::size_t const length) -> std::optional<int>
            {
                if (offset + length > value.size()) return std::nullopt;
                auto result = 0;
                for (std::size_t index{}; index < length; ++index)
                {
                    auto const character = value[offset + index];
                    if (character < L'0' || character > L'9') return std::nullopt;
                    result = result * 10 + static_cast<int>(character - L'0');
                }
                return result;
            };

            auto const yearValue = readNumber(0, 4);
            auto const monthValue = readNumber(5, 2);
            auto const dayValue = readNumber(8, 2);
            auto const hourValue = readNumber(11, 2);
            auto const minuteValue = readNumber(14, 2);
            auto const secondValue = readNumber(17, 2);
            if (!yearValue || !monthValue || !dayValue || !hourValue || !minuteValue || !secondValue)
            {
                return {};
            }

            std::chrono::nanoseconds fractional{};
            auto cursor = std::size_t{ 19 };
            if (cursor < value.size() && value[cursor] == L'.')
            {
                ++cursor;
                auto digitCount = 0u;
                std::uint32_t fraction{};
                while (cursor < value.size() && value[cursor] >= L'0' && value[cursor] <= L'9')
                {
                    if (digitCount < 9u)
                    {
                        fraction = fraction * 10u + static_cast<std::uint32_t>(value[cursor] - L'0');
                    }
                    ++digitCount;
                    ++cursor;
                }
                if (digitCount == 0u)
                {
                    return {};
                }
                while (digitCount < 9u)
                {
                    fraction *= 10u;
                    ++digitCount;
                }
                fractional = std::chrono::nanoseconds{ fraction };
            }

            auto const date = std::chrono::year_month_day{
                std::chrono::year{ *yearValue },
                std::chrono::month{ static_cast<unsigned>(*monthValue) },
                std::chrono::day{ static_cast<unsigned>(*dayValue) } };
            if (!date.ok() || *hourValue > 23 || *minuteValue > 59 || *secondValue > 60)
            {
                return {};
            }

            auto const instant = std::chrono::sys_days{ date } +
                std::chrono::hours{ *hourValue } +
                std::chrono::minutes{ *minuteValue } +
                std::chrono::seconds{ *secondValue } +
                fractional;
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(instant);
        }

        [[nodiscard]] std::vector<models::EventProperty> ExtractEventData(
            std::wstring_view const xml,
            std::wstring_view const sectionName)
        {
            std::vector<models::EventProperty> properties;
            auto const section = ElementContents(xml, sectionName);
            if (!section)
            {
                return properties;
            }

            auto cursor = section->first;
            while (cursor < section->second)
            {
                auto const dataStart = FindOpeningTag(xml, L"Data", cursor);
                if (dataStart == std::wstring_view::npos || dataStart >= section->second)
                {
                    break;
                }
                auto const dataEnd = xml.find(L'>', dataStart);
                if (dataEnd == std::wstring_view::npos || dataEnd >= section->second)
                {
                    break;
                }
                auto const tag = xml.substr(dataStart, dataEnd - dataStart + 1);
                auto const name = XmlAttribute(tag, L"Name");
                auto valueEnd = dataEnd;
                std::wstring value;
                if (dataEnd == dataStart || xml[dataEnd - 1] != L'/')
                {
                    auto const closingStart = xml.find(L"</Data>", dataEnd + 1);
                    if (closingStart == std::wstring_view::npos || closingStart > section->second)
                    {
                        break;
                    }
                    value = XmlUnescape(TrimWhitespace(xml.substr(dataEnd + 1, closingStart - dataEnd - 1)));
                    valueEnd = closingStart + 6;
                }
                auto propertyName = name ? XmlUnescape(*name) : std::wstring{};
                if (propertyName.empty())
                {
                    propertyName = L"Data " + std::to_wstring(properties.size() + 1);
                }
                properties.push_back({ std::move(propertyName), std::move(value) });
                cursor = valueEnd + 1;
            }
            return properties;
        }

        [[nodiscard]] std::wstring EventDataPreview(std::wstring_view const xml)
        {
            auto const properties = ExtractEventData(xml, L"EventData");
            std::wstring preview;
            for (std::size_t index{}; index < properties.size() && index < 2; ++index)
            {
                if (!preview.empty()) preview += L" · ";
                if (!properties[index].Name.empty())
                {
                    preview += properties[index].Name;
                    if (!properties[index].Value.empty()) preview += L": ";
                }
                preview += properties[index].Value;
            }
            constexpr auto MaximumPreviewLength = std::size_t{ 240 };
            if (preview.size() > MaximumPreviewLength)
            {
                preview.resize(MaximumPreviewLength - 1);
                preview += L"…";
            }
            return preview;
        }

        [[nodiscard]] models::EventRecordSummary ParseLiveEventSummary(std::wstring_view const xml)
        {
            models::EventRecordSummary summary;
            if (auto const provider = OpeningTag(xml, L"Provider"))
            {
                summary.Provider = XmlUnescape(XmlAttribute(*provider, L"Name").value_or(L""));
                summary.ProviderGuid = XmlUnescape(XmlAttribute(*provider, L"Guid").value_or(L""));
            }
            if (auto const eventId = ParseUnsigned(ElementText(xml, L"EventID"));
                eventId && *eventId <= (std::numeric_limits<std::uint16_t>::max)())
            {
                summary.EventId = static_cast<std::uint16_t>(*eventId);
            }
            if (auto const eventId = OpeningTag(xml, L"EventID"))
            {
                if (auto const version = ParseUnsigned(XmlAttribute(*eventId, L"Version").value_or(L""));
                    version && *version <= (std::numeric_limits<std::uint8_t>::max)())
                {
                    summary.Version = static_cast<std::uint8_t>(*version);
                }
            }
            if (auto const level = ParseUnsigned(ElementText(xml, L"Level"));
                level && *level <= (std::numeric_limits<std::uint8_t>::max)())
            {
                summary.Level = static_cast<std::uint8_t>(*level);
            }
            if (auto const opcode = ParseUnsigned(ElementText(xml, L"Opcode"));
                opcode && *opcode <= (std::numeric_limits<std::uint8_t>::max)())
            {
                summary.Opcode = static_cast<std::uint8_t>(*opcode);
            }
            if (auto const keywords = ParseUnsigned(ElementText(xml, L"Keywords"))) summary.Keywords = *keywords;
            if (auto const recordId = ParseUnsigned(ElementText(xml, L"EventRecordID"))) summary.RecordId = *recordId;
            summary.TaskDisplayName = ElementText(xml, L"Task");
            summary.Channel = ElementText(xml, L"Channel");
            summary.Computer = ElementText(xml, L"Computer");
            if (auto const timeCreated = OpeningTag(xml, L"TimeCreated"))
            {
                summary.TimeCreated = ParseEventTime(XmlAttribute(*timeCreated, L"SystemTime").value_or(L""));
            }
            if (auto const security = OpeningTag(xml, L"Security"))
            {
                summary.User = XmlUnescape(XmlAttribute(*security, L"UserID").value_or(L""));
            }
            if (auto const execution = OpeningTag(xml, L"Execution"))
            {
                if (auto const processId = ParseUnsigned(XmlAttribute(*execution, L"ProcessID").value_or(L""));
                    processId && *processId <= (std::numeric_limits<std::uint32_t>::max)())
                {
                    summary.ProcessId = static_cast<std::uint32_t>(*processId);
                }
                if (auto const threadId = ParseUnsigned(XmlAttribute(*execution, L"ThreadID").value_or(L""));
                    threadId && *threadId <= (std::numeric_limits<std::uint32_t>::max)())
                {
                    summary.ThreadId = static_cast<std::uint32_t>(*threadId);
                }
            }
            if (auto const correlation = OpeningTag(xml, L"Correlation"))
            {
                summary.ActivityId = XmlUnescape(XmlAttribute(*correlation, L"ActivityID").value_or(L""));
                summary.RelatedActivityId = XmlUnescape(XmlAttribute(*correlation, L"RelatedActivityID").value_or(L""));
            }
            summary.ShortDescription = EventDataPreview(xml);
            return summary;
        }

        [[nodiscard]] std::wstring FormatProperties(std::vector<models::EventProperty> const& properties)
        {
            std::wstring result;
            for (auto const& property : properties)
            {
                if (!result.empty()) result += L"\r\n";
                result += property.Name;
                if (!property.Value.empty())
                {
                    result += L"\t";
                    result += property.Value;
                }
            }
            return result;
        }

        [[nodiscard]] std::wstring FormatProviderMetadata(models::ProviderMetadataSnapshot const& metadata)
        {
            std::wstring result;
            auto append = [&result](std::wstring const& value)
            {
                if (value.empty()) return;
                if (!result.empty()) result += L"\r\n";
                result += value;
            };
            append(metadata.Name);
            append(metadata.Guid);
            append(metadata.ResourceFilePath);
            append(metadata.MessageFilePath);
            append(metadata.HelpLink);
            return result;
        }

        void RedactXmlElement(std::wstring& xml, std::wstring_view const elementName)
        {
            auto const openingPrefix = L"<" + std::wstring{ elementName };
            auto const closingTag = L"</" + std::wstring{ elementName } + L">";
            std::size_t searchFrom{};
            while ((searchFrom = xml.find(openingPrefix, searchFrom)) != std::wstring::npos)
            {
                auto const boundary = searchFrom + openingPrefix.size();
                if (boundary >= xml.size() || (xml[boundary] != L'>' && !std::iswspace(xml[boundary])))
                {
                    searchFrom = boundary;
                    continue;
                }
                auto const openingEnd = xml.find(L'>', boundary);
                if (openingEnd == std::wstring::npos) return;
                if (openingEnd > searchFrom && xml[openingEnd - 1] == L'/')
                {
                    searchFrom = openingEnd + 1;
                    continue;
                }
                auto const closingStart = xml.find(closingTag, openingEnd + 1);
                if (closingStart == std::wstring::npos) return;
                xml.replace(openingEnd + 1, closingStart - openingEnd - 1, L"[redacted]");
                searchFrom = openingEnd + 1 + std::wstring_view{ L"[redacted]" }.size() + closingTag.size();
            }
        }

        void RedactXmlAttribute(std::wstring& xml, std::wstring_view const attributeName)
        {
            std::size_t searchFrom{};
            while ((searchFrom = xml.find(attributeName, searchFrom)) != std::wstring::npos)
            {
                auto const before = searchFrom == 0 ? L' ' : xml[searchFrom - 1];
                auto cursor = searchFrom + attributeName.size();
                if ((!std::iswspace(before) && before != L'<') ||
                    (cursor < xml.size() && !std::iswspace(xml[cursor]) && xml[cursor] != L'='))
                {
                    searchFrom = cursor;
                    continue;
                }
                while (cursor < xml.size() && std::iswspace(xml[cursor])) ++cursor;
                if (cursor >= xml.size() || xml[cursor] != L'=')
                {
                    searchFrom = cursor;
                    continue;
                }
                ++cursor;
                while (cursor < xml.size() && std::iswspace(xml[cursor])) ++cursor;
                if (cursor >= xml.size() || (xml[cursor] != L'"' && xml[cursor] != L'\''))
                {
                    searchFrom = cursor;
                    continue;
                }
                auto const quote = xml[cursor++];
                auto const valueEnd = xml.find(quote, cursor);
                if (valueEnd == std::wstring::npos) return;
                xml.replace(cursor, valueEnd - cursor, L"[redacted]");
                searchFrom = cursor + std::wstring_view{ L"[redacted]" }.size();
            }
        }

        [[nodiscard]] bool IsSensitiveDataName(
            std::wstring value,
            bool const redactComputerName,
            bool const redactUserNames)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t const character)
                {
                    return static_cast<wchar_t>(std::towlower(character));
                });
            if (redactComputerName &&
                (value.find(L"computername") != std::wstring::npos ||
                    value.find(L"machinename") != std::wstring::npos ||
                    value.find(L"workstation") != std::wstring::npos))
            {
                return true;
            }
            return redactUserNames &&
                (value.find(L"username") != std::wstring::npos ||
                    value.find(L"accountname") != std::wstring::npos ||
                    value.find(L"membername") != std::wstring::npos ||
                    value.find(L"userid") != std::wstring::npos);
        }

        void RedactNamedEventData(
            std::wstring& xml,
            bool const redactComputerName,
            bool const redactUserNames)
        {
            std::size_t searchFrom{};
            while ((searchFrom = xml.find(L"<Data", searchFrom)) != std::wstring::npos)
            {
                auto const boundary = searchFrom + 5;
                if (boundary >= xml.size() || (xml[boundary] != L'>' && !std::iswspace(xml[boundary])))
                {
                    searchFrom = boundary;
                    continue;
                }
                auto const openingEnd = xml.find(L'>', boundary);
                if (openingEnd == std::wstring::npos) return;
                auto const name = XmlAttribute(
                    std::wstring_view{ xml }.substr(searchFrom, openingEnd - searchFrom + 1),
                    L"Name");
                auto const closingStart = xml.find(L"</Data>", openingEnd + 1);
                if (closingStart == std::wstring::npos) return;
                if (name && IsSensitiveDataName(*name, redactComputerName, redactUserNames))
                {
                    xml.replace(openingEnd + 1, closingStart - openingEnd - 1, L"[redacted]");
                    searchFrom = openingEnd + 1 + std::wstring_view{ L"[redacted]" }.size() + 7;
                }
                else
                {
                    searchFrom = closingStart + 7;
                }
            }
        }

        [[nodiscard]] std::wstring RedactLiveEventForExport(
            std::wstring xml,
            bool const redactComputerName,
            bool const redactUserNames)
        {
            if (redactComputerName)
            {
                for (auto const element : { L"Computer", L"ComputerName", L"MachineName", L"Workstation", L"WorkstationName" })
                {
                    RedactXmlElement(xml, element);
                }
            }
            if (redactUserNames)
            {
                RedactXmlAttribute(xml, L"UserID");
                for (auto const element : { L"User", L"UserName", L"TargetUserName", L"SubjectUserName", L"CallerUserName", L"AccountName", L"MemberName" })
                {
                    RedactXmlElement(xml, element);
                }
            }
            RedactNamedEventData(xml, redactComputerName, redactUserNames);
            return xml;
        }
    }

    models::LiveEventRecord WindowsEventLiveDataService::CreateRecord(std::wstring rawXml) const
    {
        models::LiveEventRecord record;
        record.Summary = ParseLiveEventSummary(rawXml);
        record.RawXml = std::move(rawXml);
        return record;
    }

    models::EventDetails WindowsEventLiveDataService::CreateFallbackDetails(
        models::LiveEventRecord const& record) const
    {
        models::EventDetails details;
        details.Summary = record.Summary;
        details.RawXml = record.RawXml;
        details.ProviderMetadata.Name = details.Summary.Provider;
        details.EventData = ExtractEventData(details.RawXml, L"EventData");
        details.UserData = ExtractEventData(details.RawXml, L"UserData");
        if (details.UserData.empty())
        {
            auto const userData = ElementText(details.RawXml, L"UserData");
            if (!userData.empty()) details.UserData.push_back({ L"UserData", userData });
        }
        details.BinaryData = ElementText(details.RawXml, L"Binary");
        if (details.BinaryData.empty()) details.BinaryData = ElementText(details.RawXml, L"BinaryEventData");
        details.RelatedEvents = details.Summary.RelatedActivityId.empty()
            ? details.Summary.ActivityId
            : details.Summary.RelatedActivityId;
        return details;
    }

    LiveEventDetailsText WindowsEventLiveDataService::CreateDetailsText(
        models::EventDetails const& details) const
    {
        return {
            details.FormattedMessage,
            details.RawXml,
            FormatProperties(details.EventData),
            FormatProperties(details.UserData),
            FormatProviderMetadata(details.ProviderMetadata),
            details.BinaryData,
            details.RelatedEvents };
    }

    LiveEventExportResult WindowsEventLiveDataService::ExportRecordedEvents(
        std::vector<std::wstring> events,
        bool const redactComputerName,
        bool const redactUserNames) const
    {
        LiveEventExportResult result;
        try
        {
            std::wstring content;
            for (auto& event : events)
            {
                content += RedactLiveEventForExport(
                    std::move(event),
                    redactComputerName,
                    redactUserNames);
                content += L"\r\n";
            }
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = folder.CreateFileAsync(
                L"Live-recording.txt",
                winrt::Windows::Storage::CreationCollisionOption::GenerateUniqueName).get();
            winrt::Windows::Storage::FileIO::WriteTextAsync(file, content).get();
            result.Path = file.Path().c_str();
        }
        catch (winrt::hresult_error const& error)
        {
            result.ErrorCode = static_cast<std::uint32_t>(error.code().value);
        }
        catch (...)
        {
            result.ErrorCode = static_cast<std::uint32_t>(E_FAIL);
        }
        return result;
    }
}
