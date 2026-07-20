// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventLogCatalogService.h"
#include "UniqueEvtHandle.h"

#include <winevt.h>

#include <string>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace AstralChronicle::services
{
    std::uint32_t WindowsEventLogCatalogService::GetAvailableChannelCount() const noexcept
    {
        unique_evt_handle channelEnumerator{ EvtOpenChannelEnum(nullptr, 0) };
        if (!channelEnumerator)
        {
            return 0;
        }

        std::uint32_t channelCount{};
        for (;;)
        {
            DWORD requiredCharacters{};
            if (EvtNextChannelPath(channelEnumerator.get(), 0, nullptr, &requiredCharacters))
            {
                ++channelCount;
                continue;
            }

            auto const error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS)
            {
                break;
            }

            if (error != ERROR_INSUFFICIENT_BUFFER || requiredCharacters == 0)
            {
                break;
            }

            std::vector<wchar_t> path(requiredCharacters);
            if (!EvtNextChannelPath(channelEnumerator.get(), requiredCharacters, path.data(), &requiredCharacters))
            {
                break;
            }

            ++channelCount;
        }

        return channelCount;
    }

    std::vector<models::EventChannelDescriptor> WindowsEventLogCatalogService::EnumerateChannels() const
    {
        std::vector<models::EventChannelDescriptor> channels;
        unique_evt_handle channelEnumerator{ EvtOpenChannelEnum(nullptr, 0) };
        if (!channelEnumerator)
        {
            return channels;
        }

        for (;;)
        {
            DWORD requiredCharacters{};
            if (EvtNextChannelPath(channelEnumerator.get(), 0, nullptr, &requiredCharacters))
            {
                continue;
            }

            auto const enumerationError = GetLastError();
            if (enumerationError == ERROR_NO_MORE_ITEMS)
            {
                break;
            }

            if (enumerationError != ERROR_INSUFFICIENT_BUFFER || requiredCharacters == 0)
            {
                break;
            }

            std::vector<wchar_t> path(requiredCharacters);
            if (!EvtNextChannelPath(
                    channelEnumerator.get(),
                    requiredCharacters,
                    path.data(),
                    &requiredCharacters))
            {
                break;
            }

            models::EventChannelDescriptor descriptor;
            descriptor.Path = path.data();

            unique_evt_handle channelConfig{ EvtOpenChannelConfig(nullptr, descriptor.Path.c_str(), 0) };
            if (!channelConfig)
            {
                descriptor.State = GetLastError() == ERROR_ACCESS_DENIED
                    ? models::EventChannelState::AccessDenied
                    : models::EventChannelState::Unavailable;
                channels.emplace_back(std::move(descriptor));
                continue;
            }

            EVT_VARIANT enabled{};
            DWORD usedBytes{};
            if (!EvtGetChannelConfigProperty(
                    channelConfig.get(),
                    EvtChannelConfigEnabled,
                    0,
                    sizeof(enabled),
                    &enabled,
                    &usedBytes))
            {
                descriptor.State = GetLastError() == ERROR_ACCESS_DENIED
                    ? models::EventChannelState::AccessDenied
                    : models::EventChannelState::Unavailable;
            }
            else
            {
                descriptor.State = enabled.BooleanVal
                    ? models::EventChannelState::Available
                    : models::EventChannelState::Disabled;
            }

            channels.emplace_back(std::move(descriptor));
        }

        return channels;
    }
}
