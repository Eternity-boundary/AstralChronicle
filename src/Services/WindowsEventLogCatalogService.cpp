// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventLogCatalogService.h"

#include <winevt.h>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace AstralChronicle::services
{
    std::uint32_t WindowsEventLogCatalogService::GetAvailableChannelCount() const noexcept
    {
        EVT_HANDLE const channelEnumerator = EvtOpenChannelEnum(nullptr, 0);
        if (!channelEnumerator)
        {
            return 0;
        }

        std::uint32_t channelCount{};
        for (;;)
        {
            DWORD requiredCharacters{};
            if (EvtNextChannelPath(channelEnumerator, 0, nullptr, &requiredCharacters))
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
            if (!EvtNextChannelPath(channelEnumerator, requiredCharacters, path.data(), &requiredCharacters))
            {
                break;
            }

            ++channelCount;
        }

        EvtClose(channelEnumerator);
        return channelCount;
    }
}
