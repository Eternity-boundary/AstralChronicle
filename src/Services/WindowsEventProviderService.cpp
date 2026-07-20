// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventProviderService.h"
#include "UniqueEvtHandle.h"

#include <winevt.h>

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace
{
    using AstralChronicle::services::EventQueryStatus;
    using AstralChronicle::services::QueryCancellation;
    using AstralChronicle::services::unique_evt_handle;

    [[nodiscard]] std::wstring Lowercase(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character)
        {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }

    [[nodiscard]] EventQueryStatus MapError(DWORD const error)
    {
        switch (error)
        {
        case ERROR_ACCESS_DENIED:
            return EventQueryStatus::AccessDenied;
        case ERROR_SERVICE_NOT_ACTIVE:
            return EventQueryStatus::ServiceUnavailable;
        case ERROR_CANCELLED:
            return EventQueryStatus::Cancelled;
        default:
            return EventQueryStatus::Failed;
        }
    }

    [[nodiscard]] std::wstring GetStringProperty(
        EVT_HANDLE const metadata,
        EVT_PUBLISHER_METADATA_PROPERTY_ID const propertyId)
    {
        DWORD bufferBytes{};
        if (EvtGetPublisherMetadataProperty(metadata, propertyId, 0, 0, nullptr, &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }

        std::vector<std::byte> buffer(bufferBytes);
        auto const value = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtGetPublisherMetadataProperty(
                metadata,
                propertyId,
                0,
                bufferBytes,
                value,
                &bufferBytes) ||
            value->Type != EvtVarTypeString || !value->StringVal)
        {
            return {};
        }
        return value->StringVal;
    }

    [[nodiscard]] std::wstring GetGuidProperty(
        EVT_HANDLE const metadata,
        EVT_PUBLISHER_METADATA_PROPERTY_ID const propertyId)
    {
        DWORD bufferBytes{};
        if (EvtGetPublisherMetadataProperty(metadata, propertyId, 0, 0, nullptr, &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }

        std::vector<std::byte> buffer(bufferBytes);
        auto const value = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtGetPublisherMetadataProperty(
                metadata,
                propertyId,
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

    [[nodiscard]] std::uint32_t GetUInt32Property(
        EVT_EVENT_METADATA_PROPERTY_ID const propertyId,
        EVT_HANDLE const eventMetadata)
    {
        DWORD bufferBytes{};
        if (EvtGetEventMetadataProperty(eventMetadata, propertyId, 0, 0, nullptr, &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return 0;
        }
        std::vector<std::byte> buffer(bufferBytes);
        auto const value = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtGetEventMetadataProperty(eventMetadata, propertyId, 0, bufferBytes, value, &bufferBytes))
        {
            return 0;
        }
        return value->Type == EvtVarTypeUInt32 ? value->UInt32Val : 0;
    }

    [[nodiscard]] std::uint64_t GetUInt64Property(
        EVT_EVENT_METADATA_PROPERTY_ID const propertyId,
        EVT_HANDLE const eventMetadata)
    {
        DWORD bufferBytes{};
        if (EvtGetEventMetadataProperty(eventMetadata, propertyId, 0, 0, nullptr, &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return 0;
        }
        std::vector<std::byte> buffer(bufferBytes);
        auto const value = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtGetEventMetadataProperty(eventMetadata, propertyId, 0, bufferBytes, value, &bufferBytes))
        {
            return 0;
        }
        return value->Type == EvtVarTypeUInt64 ? value->UInt64Val : 0;
    }

    [[nodiscard]] std::wstring GetEventTemplate(EVT_HANDLE const eventMetadata)
    {
        DWORD bufferBytes{};
        if (EvtGetEventMetadataProperty(
                eventMetadata,
                EventMetadataEventTemplate,
                0,
                0,
                nullptr,
                &bufferBytes) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || bufferBytes == 0)
        {
            return {};
        }
        std::vector<std::byte> buffer(bufferBytes);
        auto const value = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtGetEventMetadataProperty(
                eventMetadata,
                EventMetadataEventTemplate,
                0,
                bufferBytes,
                value,
                &bufferBytes) ||
            value->Type != EvtVarTypeString || !value->StringVal)
        {
            return {};
        }
        return value->StringVal;
    }
}

namespace AstralChronicle::services
{
    EventProviderQueryResult WindowsEventProviderService::QueryProviders(
        std::wstring_view searchText,
        std::uint32_t const maximumProviders,
        QueryCancellation const& cancellation) const
    {
        EventProviderQueryResult result;
        if (maximumProviders == 0)
        {
            result.Status = EventQueryStatus::NoEvents;
            return result;
        }

        unique_evt_handle publisherEnum{ EvtOpenPublisherEnum(nullptr, 0) };
        if (!publisherEnum)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapError(result.ErrorCode);
            return result;
        }

        auto const needle = Lowercase(std::wstring{ searchText });
        while (result.Providers.size() < maximumProviders)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            DWORD requiredCharacters{};
            if (EvtNextPublisherId(publisherEnum.get(), 0, nullptr, &requiredCharacters))
            {
                continue;
            }
            auto const error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (error != ERROR_INSUFFICIENT_BUFFER || requiredCharacters == 0)
            {
                result.ErrorCode = error;
                result.Status = MapError(error);
                return result;
            }

            std::vector<wchar_t> name(requiredCharacters);
            if (!EvtNextPublisherId(
                    publisherEnum.get(),
                    requiredCharacters,
                    name.data(),
                    &requiredCharacters))
            {
                result.ErrorCode = GetLastError();
                result.Status = MapError(result.ErrorCode);
                return result;
            }

            std::wstring providerName{name.data()};
            if (needle.empty() || Lowercase(providerName).find(needle) != std::wstring::npos)
            {
                result.Providers.push_back(models::EventProviderSummary{ std::move(providerName) });
            }
        }

        result.Status = result.Providers.empty()
            ? EventQueryStatus::NoEvents
            : EventQueryStatus::Succeeded;
        return result;
    }

    EventProviderDetailsResult WindowsEventProviderService::QueryDetails(
        std::wstring_view providerName,
        QueryCancellation const& cancellation) const
    {
        EventProviderDetailsResult result;
        result.Details.Name = providerName;
        if (providerName.empty())
        {
            result.Status = EventQueryStatus::InvalidChannel;
            result.ErrorCode = ERROR_INVALID_NAME;
            return result;
        }

        {
            std::scoped_lock lock{ m_cacheMutex };
            auto const cached = m_detailsCache.find(std::wstring{ providerName });
            if (cached != m_detailsCache.end())
            {
                result.Details = cached->second;
                result.Status = EventQueryStatus::Succeeded;
                return result;
            }
        }

        unique_evt_handle metadata{ EvtOpenPublisherMetadata(nullptr, std::wstring{ providerName }.c_str(), nullptr, 0, 0) };
        if (!metadata)
        {
            result.ErrorCode = GetLastError();
            result.Status = MapError(result.ErrorCode);
            result.Details.ErrorCode = result.ErrorCode;
            return result;
        }

        if (cancellation && cancellation->load(std::memory_order_relaxed))
        {
            result.Status = EventQueryStatus::Cancelled;
            result.ErrorCode = ERROR_CANCELLED;
            return result;
        }

        result.Details.Guid = GetGuidProperty(metadata.get(), EvtPublisherMetadataPublisherGuid);
        result.Details.ResourceFilePath = GetStringProperty(metadata.get(), EvtPublisherMetadataResourceFilePath);
        result.Details.ParameterFilePath = GetStringProperty(metadata.get(), EvtPublisherMetadataParameterFilePath);
        result.Details.MessageFilePath = GetStringProperty(metadata.get(), EvtPublisherMetadataMessageFilePath);
        result.Details.HelpLink = GetStringProperty(metadata.get(), EvtPublisherMetadataHelpLink);
        result.Details.MetadataAvailable = true;

        unique_evt_handle eventEnum{ EvtOpenEventMetadataEnum(metadata.get(), 0) };
        while (eventEnum)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                result.Status = EventQueryStatus::Cancelled;
                result.ErrorCode = ERROR_CANCELLED;
                return result;
            }

            unique_evt_handle eventMetadata{ EvtNextEventMetadata(eventEnum.get(), 0) };
            if (!eventMetadata)
            {
                if (GetLastError() == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                result.ErrorCode = GetLastError();
                break;
            }

            models::EventProviderEventDefinition definition;
            definition.EventId = GetUInt32Property(EventMetadataEventID, eventMetadata.get());
            definition.Version = GetUInt32Property(EventMetadataEventVersion, eventMetadata.get());
            definition.Channel = GetUInt32Property(EventMetadataEventChannel, eventMetadata.get());
            definition.Level = GetUInt32Property(EventMetadataEventLevel, eventMetadata.get());
            definition.Opcode = GetUInt32Property(EventMetadataEventOpcode, eventMetadata.get());
            definition.Task = GetUInt32Property(EventMetadataEventTask, eventMetadata.get());
            definition.Keyword = GetUInt64Property(EventMetadataEventKeyword, eventMetadata.get());
            definition.Template = GetEventTemplate(eventMetadata.get());
            result.Details.EventDefinitions.emplace_back(std::move(definition));
        }

        result.Status = result.ErrorCode == 0
            ? EventQueryStatus::Succeeded
            : MapError(result.ErrorCode);
        if (result.Status == EventQueryStatus::Succeeded)
        {
            std::scoped_lock lock{ m_cacheMutex };
            if (m_detailsCache.size() >= 64) m_detailsCache.erase(m_detailsCache.begin());
            m_detailsCache[std::wstring{ providerName }] = result.Details;
        }
        return result;
    }
}
