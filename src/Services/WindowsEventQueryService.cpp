// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventQueryService.h"
#include "EvtVariantReader.h"
#include "UniqueEvtHandle.h"

#include <winevt.h>
#include <sddl.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <ratio>
#include <string>
#include <vector>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace
{
    using AstralChronicle::services::unique_evt_handle;

    class unique_local_string final
    {
    public:
        unique_local_string() noexcept = default;
        ~unique_local_string()
        {
            if (m_value)
            {
                LocalFree(m_value);
            }
        }

        unique_local_string(unique_local_string const&) = delete;
        unique_local_string& operator=(unique_local_string const&) = delete;

        [[nodiscard]] LPWSTR* put() noexcept { return &m_value; }
        [[nodiscard]] LPWSTR get() const noexcept { return m_value; }

    private:
        LPWSTR m_value{};
    };

    constexpr std::uint64_t WindowsEpochOffset100Nanoseconds = 116444736000000000ULL;

    [[nodiscard]] AstralChronicle::services::EventQueryStatus MapQueryError(DWORD const error)
    {
        switch (error)
        {
        case ERROR_ACCESS_DENIED:
            return AstralChronicle::services::EventQueryStatus::AccessDenied;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_EVT_CHANNEL_NOT_FOUND:
            return AstralChronicle::services::EventQueryStatus::InvalidChannel;
        case ERROR_SERVICE_NOT_ACTIVE:
            return AstralChronicle::services::EventQueryStatus::ServiceUnavailable;
        case ERROR_CANCELLED:
            return AstralChronicle::services::EventQueryStatus::Cancelled;
        default:
            return AstralChronicle::services::EventQueryStatus::Failed;
        }
    }

    [[nodiscard]] std::chrono::system_clock::time_point TimePointFromWindowsTicks(
        std::uint64_t const windowsTicks)
    {
        if (windowsTicks < WindowsEpochOffset100Nanoseconds)
        {
            return {};
        }

        using WindowsDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
        auto const unixTicks = windowsTicks - WindowsEpochOffset100Nanoseconds;
        if (unixTicks > static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)()))
        {
            return {};
        }
        return std::chrono::system_clock::time_point{
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                WindowsDuration{ static_cast<std::int64_t>(unixTicks) }) };
    }

    [[nodiscard]] std::optional<AstralChronicle::models::EventRecordSummary> RenderSummary(
        EVT_HANDLE const eventHandle,
        EVT_HANDLE const renderContext,
        DWORD& errorCode)
    {
        errorCode = ERROR_SUCCESS;
        if (!renderContext)
        {
            errorCode = ERROR_INVALID_HANDLE;
            return std::nullopt;
        }

        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(renderContext, eventHandle, EvtRenderEventValues, 0, nullptr, &bufferBytes, &propertyCount))
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }
        errorCode = GetLastError();
        if (errorCode != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return std::nullopt;
        }

        std::vector<EVT_VARIANT> buffer(
            (static_cast<std::size_t>(bufferBytes) + sizeof(EVT_VARIANT) - 1) /
            sizeof(EVT_VARIANT));
        auto const values = buffer.data();
        if (!EvtRender(renderContext, eventHandle, EvtRenderEventValues, bufferBytes, values, &bufferBytes, &propertyCount))
        {
            errorCode = GetLastError();
            return std::nullopt;
        }
        errorCode = ERROR_SUCCESS;

        using AstralChronicle::services::details::ReadEventId;
        using AstralChronicle::services::details::ReadKeywords;
        using AstralChronicle::services::details::TryGetSystemVariant;
        auto const provider = TryGetSystemVariant(values, propertyCount, EvtSystemProviderName, EvtVarTypeString);
        auto const eventId = ReadEventId(values, propertyCount);
        auto const recordId = TryGetSystemVariant(values, propertyCount, EvtSystemEventRecordId, EvtVarTypeUInt64);
        if (!provider || !provider->StringVal || !eventId || !recordId)
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }

        AstralChronicle::models::EventRecordSummary summary;
        summary.Provider = provider->StringVal;
        summary.EventId = *eventId;
        summary.RecordId = recordId->UInt64Val;

        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemProviderGuid, EvtVarTypeGuid);
            value && value->GuidVal)
        {
            wchar_t guid[64]{};
            if (StringFromGUID2(*value->GuidVal, guid, ARRAYSIZE(guid)) != 0)
            {
                summary.ProviderGuid = guid;
            }
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemLevel, EvtVarTypeByte))
        {
            summary.Level = value->ByteVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemVersion, EvtVarTypeByte))
        {
            summary.Version = value->ByteVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemOpcode, EvtVarTypeByte))
        {
            summary.Opcode = value->ByteVal;
        }
        if (auto const keywords = ReadKeywords(values, propertyCount))
        {
            summary.Keywords = *keywords;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemTimeCreated, EvtVarTypeFileTime))
        {
            summary.TimeCreated = TimePointFromWindowsTicks(value->FileTimeVal);
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemChannel, EvtVarTypeString);
            value && value->StringVal)
        {
            summary.Channel = value->StringVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemTask, EvtVarTypeUInt16))
        {
            summary.TaskDisplayName = std::to_wstring(value->UInt16Val);
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemComputer, EvtVarTypeString);
            value && value->StringVal)
        {
            summary.Computer = value->StringVal;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemUserID, EvtVarTypeSid);
            value && value->SidVal)
        {
            unique_local_string userSid;
            if (ConvertSidToStringSidW(value->SidVal, userSid.put()))
            {
                summary.User = userSid.get();
            }
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemActivityID, EvtVarTypeGuid);
            value && value->GuidVal)
        {
            wchar_t guid[64]{};
            if (StringFromGUID2(*value->GuidVal, guid, ARRAYSIZE(guid)) != 0)
            {
                summary.ActivityId = guid;
            }
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemRelatedActivityID, EvtVarTypeGuid);
            value && value->GuidVal)
        {
            wchar_t guid[64]{};
            if (StringFromGUID2(*value->GuidVal, guid, ARRAYSIZE(guid)) != 0)
            {
                summary.RelatedActivityId = guid;
            }
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemProcessID, EvtVarTypeUInt32))
        {
            summary.ProcessId = value->UInt32Val;
        }
        if (auto const value = TryGetSystemVariant(values, propertyCount, EvtSystemThreadID, EvtVarTypeUInt32))
        {
            summary.ThreadId = value->UInt32Val;
        }
        return summary;
    }

    [[nodiscard]] std::wstring ExtractXmlSection(
        std::wstring const& xml,
        std::wstring_view elementName)
    {
        auto const opening = xml.find(L"<" + std::wstring{ elementName });
        if (opening == std::wstring::npos)
        {
            return {};
        }

        auto const contentStart = xml.find(L'>', opening);
        auto const closing = xml.find(L"</" + std::wstring{ elementName } + L'>', contentStart);
        if (contentStart == std::wstring::npos || closing == std::wstring::npos)
        {
            return {};
        }
        return xml.substr(contentStart + 1, closing - contentStart - 1);
    }

    [[nodiscard]] std::vector<AstralChronicle::models::EventProperty> ExtractEventData(
        std::wstring const& xml,
        std::wstring_view sectionName)
    {
        std::vector<AstralChronicle::models::EventProperty> properties;
        auto const section = ExtractXmlSection(xml, sectionName);
        if (section.empty())
        {
            return properties;
        }

        std::size_t cursor{};
        while ((cursor = section.find(L"<Data", cursor)) != std::wstring::npos)
        {
            auto const tagEnd = section.find(L'>', cursor);
            auto const close = section.find(L"</Data>", tagEnd);
            if (tagEnd == std::wstring::npos || close == std::wstring::npos)
            {
                break;
            }

            auto const nameStart = section.find(L"Name=\"", cursor);
            std::wstring name;
            if (nameStart != std::wstring::npos && nameStart < tagEnd)
            {
                auto const nameValueStart = nameStart + 6;
                auto const nameEnd = section.find(L'\"', nameValueStart);
                if (nameEnd != std::wstring::npos && nameEnd < tagEnd)
                {
                    name = section.substr(nameValueStart, nameEnd - nameValueStart);
                }
            }
            if (name.empty())
            {
                name = L"Data";
            }
            properties.push_back({ name, section.substr(tagEnd + 1, close - tagEnd - 1) });
            cursor = close + 7;
        }
        if (properties.empty())
        {
            properties.push_back({ std::wstring{ sectionName }, section });
        }
        return properties;
    }

    [[nodiscard]] std::wstring GetPublisherStringProperty(
        EVT_HANDLE const metadata,
        EVT_PUBLISHER_METADATA_PROPERTY_ID const propertyId)
    {
        DWORD bufferBytes{};
        if (EvtGetPublisherMetadataProperty(metadata, propertyId, 0, 0, nullptr, &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }

        std::vector<EVT_VARIANT> buffer(
            (static_cast<std::size_t>(bufferBytes) + sizeof(EVT_VARIANT) - 1) /
            sizeof(EVT_VARIANT));
        auto const value = buffer.data();
        if (!EvtGetPublisherMetadataProperty(metadata, propertyId, 0, bufferBytes, value, &bufferBytes) ||
            value->Type != EvtVarTypeString || !value->StringVal)
        {
            return {};
        }
        return value->StringVal;
    }

    [[nodiscard]] std::wstring GetPublisherGuidProperty(EVT_HANDLE const metadata)
    {
        DWORD bufferBytes{};
        if (EvtGetPublisherMetadataProperty(
                metadata,
                EvtPublisherMetadataPublisherGuid,
                0,
                0,
                nullptr,
                &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }

        std::vector<EVT_VARIANT> buffer(
            (static_cast<std::size_t>(bufferBytes) + sizeof(EVT_VARIANT) - 1) /
            sizeof(EVT_VARIANT));
        auto const value = buffer.data();
        if (!EvtGetPublisherMetadataProperty(
                metadata,
                EvtPublisherMetadataPublisherGuid,
                0,
                bufferBytes,
                value,
                &bufferBytes) ||
            value->Type != EvtVarTypeGuid || !value->GuidVal)
        {
            return {};
        }

        wchar_t guid[64]{};
        return StringFromGUID2(*value->GuidVal, guid, ARRAYSIZE(guid)) == 0
            ? std::wstring{}
            : std::wstring{ guid };
    }

    void PopulateDetails(
        AstralChronicle::models::EventDetails& details,
        std::wstring const& xml)
    {
        details.SystemProperties = {
            { L"Provider", details.Summary.Provider },
            { L"ProviderGuid", details.Summary.ProviderGuid },
            { L"EventRecordID", std::to_wstring(details.Summary.RecordId) },
            { L"EventID", std::to_wstring(details.Summary.EventId) },
            { L"Version", std::to_wstring(details.Summary.Version) },
            { L"Level", std::to_wstring(details.Summary.Level) },
            { L"Opcode", std::to_wstring(details.Summary.Opcode) },
            { L"Keywords", std::to_wstring(details.Summary.Keywords) },
            { L"ProcessID", std::to_wstring(details.Summary.ProcessId) },
            { L"ThreadID", std::to_wstring(details.Summary.ThreadId) },
            { L"Computer", details.Summary.Computer },
            { L"UserID", details.Summary.User },
            { L"ActivityID", details.Summary.ActivityId },
            { L"RelatedActivityID", details.Summary.RelatedActivityId } };
        details.EventData = ExtractEventData(xml, L"EventData");
        details.UserData = ExtractEventData(xml, L"UserData");
        details.BinaryData = ExtractXmlSection(xml, L"BinaryEventData");
        details.RelatedEvents = details.Summary.RelatedActivityId.empty()
            ? details.Summary.ActivityId
            : details.Summary.RelatedActivityId;
    }

    [[nodiscard]] std::optional<std::uint8_t> RenderLevel(
        EVT_HANDLE const eventHandle,
        EVT_HANDLE const renderContext,
        DWORD& errorCode)
    {
        errorCode = ERROR_SUCCESS;
        if (!renderContext)
        {
            errorCode = ERROR_INVALID_HANDLE;
            return std::nullopt;
        }

        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(renderContext, eventHandle, EvtRenderEventValues, 0, nullptr, &bufferBytes, &propertyCount))
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }
        errorCode = GetLastError();
        if (errorCode != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return std::nullopt;
        }

        std::vector<EVT_VARIANT> buffer(
            (static_cast<std::size_t>(bufferBytes) + sizeof(EVT_VARIANT) - 1) /
            sizeof(EVT_VARIANT));
        auto const values = buffer.data();
        if (!EvtRender(renderContext, eventHandle, EvtRenderEventValues, bufferBytes, values, &bufferBytes, &propertyCount))
        {
            errorCode = GetLastError();
            return std::nullopt;
        }

        errorCode = ERROR_SUCCESS;
        auto const level = AstralChronicle::services::details::TryGetSystemVariant(
            values,
            propertyCount,
            EvtSystemLevel,
            EvtVarTypeByte);
        return level ? level->ByteVal : 0;
    }

    [[nodiscard]] std::optional<std::wstring> RenderXml(
        EVT_HANDLE const eventHandle,
        DWORD& errorCode)
    {
        errorCode = ERROR_SUCCESS;
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(nullptr, eventHandle, EvtRenderEventXml, 0, nullptr, &bufferBytes, &propertyCount))
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }
        errorCode = GetLastError();
        if (errorCode != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return std::nullopt;
        }

        std::vector<wchar_t> buffer((bufferBytes / sizeof(wchar_t)) + 1);
        if (!EvtRender(
                nullptr,
                eventHandle,
                EvtRenderEventXml,
                bufferBytes,
                buffer.data(),
                &bufferBytes,
                &propertyCount))
        {
            errorCode = GetLastError();
            return std::nullopt;
        }
        if (buffer.front() == L'\0')
        {
            errorCode = ERROR_EVT_INVALID_EVENT_DATA;
            return std::nullopt;
        }
        errorCode = ERROR_SUCCESS;
        return std::wstring{ buffer.data() };
    }

    [[nodiscard]] std::wstring FormatMessage(
        EVT_HANDLE const eventHandle,
        std::wstring_view provider,
        std::uint32_t& formattingErrorCode)
    {
        formattingErrorCode = 0;
        if (provider.empty())
        {
            formattingErrorCode = ERROR_EVT_PUBLISHER_METADATA_NOT_FOUND;
            return {};
        }

        unique_evt_handle metadata{ EvtOpenPublisherMetadata(nullptr, std::wstring{ provider }.c_str(), nullptr, 0, 0) };
        if (!metadata)
        {
            formattingErrorCode = GetLastError();
            return {};
        }

        DWORD bufferCharacters{};
        DWORD usedCharacters{};
        if (EvtFormatMessage(
                metadata.get(),
                eventHandle,
                0,
                0,
                nullptr,
                EvtFormatMessageEvent,
                0,
                nullptr,
                &bufferCharacters))
        {
            return {};
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferCharacters == 0)
        {
            formattingErrorCode = GetLastError();
            return {};
        }

        std::vector<wchar_t> buffer(bufferCharacters + 1);
        if (!EvtFormatMessage(
                metadata.get(),
                eventHandle,
                0,
                0,
                nullptr,
                EvtFormatMessageEvent,
                static_cast<DWORD>(buffer.size()),
                buffer.data(),
                &usedCharacters))
        {
            formattingErrorCode = GetLastError();
            return {};
        }

        return std::wstring{ buffer.data(), usedCharacters > 0 ? usedCharacters - 1 : 0 };
    }
}

namespace AstralChronicle::services
{
    EventQueryResult WindowsEventQueryService::QueryPage(
        std::wstring_view channel,
        std::uint32_t maximumRecords,
        bool reverseDirection,
        QueryCancellation const& cancellation) const
    {
        return QueryPageWithQuery(channel, L"*", maximumRecords, reverseDirection, cancellation);
    }

    EventQueryResult WindowsEventQueryService::QueryPageWithQuery(
        std::wstring_view channel,
        std::wstring_view queryText,
        std::uint32_t maximumRecords,
        bool reverseDirection,
        QueryCancellation const& cancellation) const
    {
        EventQueryResult result;
        auto const queryStart = queryText.find_first_not_of(L" \t\r\n");
        auto const isStructuredQuery = queryStart != std::wstring_view::npos &&
            queryText.compare(queryStart, 10, L"<QueryList") == 0;
        if (queryText.empty() || maximumRecords == 0 || (channel.empty() && !isStructuredQuery))
        {
            result.Status = EventQueryStatus::InvalidChannel;
            result.ErrorCode = ERROR_INVALID_NAME;
            return result;
        }

        DWORD queryFlags{};
        if (reverseDirection)
        {
            queryFlags = EvtQueryReverseDirection;
        }

        std::wstring const query{ queryText };
        unique_evt_handle queryHandle;
        if (channel.empty())
        {
            queryHandle.reset(EvtQuery(nullptr, nullptr, query.c_str(), queryFlags));
        }
        else
        {
            queryFlags |= EvtQueryChannelPath;
            std::wstring const channelPath{ channel };
            queryHandle.reset(EvtQuery(nullptr, channelPath.c_str(), query.c_str(), queryFlags));
        }
        if (!queryHandle)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        unique_evt_handle renderContext{ EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem) };
        if (!renderContext)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        auto const batchSize = std::min<std::uint32_t>(maximumRecords, 64u);
        std::vector<EVT_HANDLE> events(batchSize);
        std::vector<unique_evt_handle> ownedEvents(batchSize);
        result.Events.reserve(maximumRecords);

        while (result.Events.size() < maximumRecords)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                EvtCancel(queryHandle.get());
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            DWORD returned{};
            std::fill(events.begin(), events.end(), nullptr);
            for (auto& event : ownedEvents) event.reset();
            auto const succeeded = EvtNext(
                queryHandle.get(),
                static_cast<DWORD>(std::min<std::size_t>(events.size(), maximumRecords - result.Events.size())),
                events.data(),
                100,
                0,
                &returned);
            auto const error = succeeded ? ERROR_SUCCESS : GetLastError();
            for (std::size_t index{}; index < events.size(); ++index)
            {
                ownedEvents[index].reset(events[index]);
            }

            if (!succeeded)
            {
                if (error == ERROR_TIMEOUT)
                {
                    continue;
                }
                if (error == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                if (cancellation && cancellation->load(std::memory_order_relaxed))
                {
                    result.Status = EventQueryStatus::Cancelled;
                    result.ErrorCode = ERROR_CANCELLED;
                    return result;
                }
                result.ErrorCode = error;
                result.Status = MapQueryError(error);
                return result;
            }

            for (DWORD index{}; index < returned; ++index)
            {
                DWORD renderError{};
                auto summary = RenderSummary(
                    ownedEvents[index].get(),
                    renderContext.get(),
                    renderError);
                if (!summary)
                {
                    result.Status = EventQueryStatus::Failed;
                    result.ErrorCode = renderError == ERROR_SUCCESS
                        ? ERROR_EVT_INVALID_EVENT_DATA
                        : renderError;
                    return result;
                }
                result.Events.emplace_back(std::move(*summary));
            }

            if (returned == 0)
            {
                break;
            }
        }

        result.Status = result.Events.empty()
            ? EventQueryStatus::NoEvents
            : EventQueryStatus::Succeeded;
        return result;
    }

    EventLevelCountsResult WindowsEventQueryService::QueryLevelCounts(
        std::wstring_view channel,
        std::wstring_view queryText,
        QueryCancellation const& cancellation) const
    {
        EventLevelCountsResult result;
        if (channel.empty() || queryText.empty())
        {
            result.Status = EventQueryStatus::InvalidChannel;
            result.ErrorCode = ERROR_INVALID_NAME;
            return result;
        }

        std::wstring const channelPath{ channel };
        std::wstring const query{ queryText };
        unique_evt_handle queryHandle{ EvtQuery(nullptr, channelPath.c_str(), query.c_str(), EvtQueryChannelPath) };
        if (!queryHandle)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        unique_evt_handle renderContext{ EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem) };
        if (!renderContext)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        constexpr DWORD batchSize = 64;
        std::vector<EVT_HANDLE> events(batchSize);
        std::vector<unique_evt_handle> ownedEvents(batchSize);
        for (;;)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                EvtCancel(queryHandle.get());
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            DWORD returned{};
            std::fill(events.begin(), events.end(), nullptr);
            for (auto& event : ownedEvents) event.reset();
            auto const succeeded = EvtNext(queryHandle.get(), batchSize, events.data(), 100, 0, &returned);
            auto const error = succeeded ? ERROR_SUCCESS : GetLastError();
            for (std::size_t index{}; index < events.size(); ++index)
            {
                ownedEvents[index].reset(events[index]);
            }

            if (!succeeded)
            {
                if (error == ERROR_TIMEOUT)
                {
                    continue;
                }
                if (error == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                result.ErrorCode = error;
                result.Status = MapQueryError(error);
                return result;
            }

            for (DWORD index{}; index < returned; ++index)
            {
                DWORD renderError{};
                auto const level = RenderLevel(
                    ownedEvents[index].get(),
                    renderContext.get(),
                    renderError);
                if (!level)
                {
                    result.Status = EventQueryStatus::Failed;
                    result.ErrorCode = renderError == ERROR_SUCCESS
                        ? ERROR_EVT_INVALID_EVENT_DATA
                        : renderError;
                    return result;
                }
                ++result.Counts.Total;
                switch (*level)
                {
                case 1:
                    ++result.Counts.Critical;
                    break;
                case 2:
                    ++result.Counts.Error;
                    break;
                case 3:
                    ++result.Counts.Warning;
                    break;
                default:
                    break;
                }
            }
        }

        result.Status = result.Counts.Total == 0
            ? EventQueryStatus::NoEvents
            : EventQueryStatus::Succeeded;
        return result;
    }

    EventDetailsResult WindowsEventQueryService::QueryDetails(
        std::wstring_view channel,
        std::uint64_t const recordId,
        QueryCancellation const& cancellation) const
    {
        EventDetailsResult result;
        if (channel.empty() || recordId == 0)
        {
            result.Status = EventQueryStatus::InvalidChannel;
            result.ErrorCode = ERROR_INVALID_NAME;
            return result;
        }

        std::wstring const queryText = L"*[System[EventRecordID=" + std::to_wstring(recordId) + L"]]";
        unique_evt_handle query{ EvtQuery(nullptr, std::wstring{ channel }.c_str(), queryText.c_str(), EvtQueryChannelPath) };
        if (!query)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        unique_evt_handle renderContext{ EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem) };
        if (!renderContext)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapQueryError(result.ErrorCode);
            return result;
        }

        unique_evt_handle event;
        for (;;)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                EvtCancel(query.get());
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            EVT_HANDLE eventValue{};
            DWORD returned{};
            auto const succeeded = EvtNext(query.get(), 1, &eventValue, 100, 0, &returned);
            auto const error = succeeded ? ERROR_SUCCESS : GetLastError();
            unique_evt_handle candidate{ eventValue };
            if (succeeded)
            {
                if (returned != 1 || !candidate)
                {
                    result.Status = EventQueryStatus::Failed;
                    result.ErrorCode = ERROR_EVT_INVALID_EVENT_DATA;
                    return result;
                }
                event = std::move(candidate);
                break;
            }
            if (error == ERROR_TIMEOUT)
            {
                continue;
            }
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }
            result.ErrorCode = error;
            result.Status = error == ERROR_NO_MORE_ITEMS
                ? EventQueryStatus::NoEvents
                : MapQueryError(error);
            return result;
        }

        DWORD summaryError{};
        auto summary = RenderSummary(event.get(), renderContext.get(), summaryError);
        if (!summary)
        {
            result.Status = EventQueryStatus::Failed;
            result.ErrorCode = summaryError == ERROR_SUCCESS
                ? ERROR_EVT_INVALID_EVENT_DATA
                : summaryError;
            return result;
        }
        DWORD xmlError{};
        auto rawXml = RenderXml(event.get(), xmlError);
        if (!rawXml)
        {
            result.Status = EventQueryStatus::Failed;
            result.ErrorCode = xmlError == ERROR_SUCCESS
                ? ERROR_EVT_INVALID_EVENT_DATA
                : xmlError;
            return result;
        }
        result.Details.Summary = std::move(*summary);
        result.Details.RawXml = std::move(*rawXml);
        PopulateDetails(result.Details, result.Details.RawXml);
        result.Details.ProviderMetadata.Name = result.Details.Summary.Provider;
        result.Details.FormattedMessage = FormatMessage(
            event.get(),
            result.Details.Summary.Provider,
            result.Details.FormattingErrorCode);
        unique_evt_handle metadata{
            EvtOpenPublisherMetadata(
                nullptr,
                result.Details.Summary.Provider.c_str(),
                nullptr,
                0,
                0) };
        if (metadata)
        {
            result.Details.ProviderMetadata.IsAvailable = true;
            result.Details.ProviderMetadata.Guid = GetPublisherGuidProperty(metadata.get());
            result.Details.ProviderMetadata.ResourceFilePath = GetPublisherStringProperty(
                metadata.get(), EvtPublisherMetadataResourceFilePath);
            result.Details.ProviderMetadata.MessageFilePath = GetPublisherStringProperty(
                metadata.get(), EvtPublisherMetadataMessageFilePath);
            result.Details.ProviderMetadata.HelpLink = GetPublisherStringProperty(
                metadata.get(), EvtPublisherMetadataHelpLink);
        }
        result.Details.Summary.ShortDescription = result.Details.FormattedMessage;
        result.Status = EventQueryStatus::Succeeded;
        return result;
    }

    std::vector<models::EventRecordSummary> WindowsEventQueryService::QueryRecent(
        std::wstring_view channel,
        std::uint32_t maximumRecords) const
    {
        return IEventQueryService::QueryRecent(channel, maximumRecords);
    }
}
