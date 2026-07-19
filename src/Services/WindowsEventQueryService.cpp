// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsEventQueryService.h"

#include <winevt.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

namespace
{
    class EventHandle final
    {
    public:
        explicit EventHandle(EVT_HANDLE handle = nullptr) noexcept : m_handle(handle) {}
        ~EventHandle()
        {
            if (m_handle)
            {
                EvtClose(m_handle);
            }
        }

        EventHandle(EventHandle const&) = delete;
        EventHandle& operator=(EventHandle const&) = delete;

        [[nodiscard]] EVT_HANDLE Get() const noexcept { return m_handle; }

    private:
        EVT_HANDLE m_handle{};
    };

    [[nodiscard]] AstralChronicle::models::EventRecordSummary RenderSummary(EVT_HANDLE eventHandle)
    {
        DWORD bufferBytes{};
        DWORD propertyCount{};
        if (EvtRender(nullptr, eventHandle, EvtRenderEventValues, 0, nullptr, &bufferBytes, &propertyCount) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            return {};
        }

        std::vector<std::byte> buffer(bufferBytes);
        auto const values = reinterpret_cast<PEVT_VARIANT>(buffer.data());
        if (!EvtRender(nullptr, eventHandle, EvtRenderEventValues, bufferBytes, values, &bufferBytes, &propertyCount))
        {
            return {};
        }

        AstralChronicle::models::EventRecordSummary summary;
        if (propertyCount > EvtSystemProviderName && values[EvtSystemProviderName].StringVal)
        {
            summary.Provider = values[EvtSystemProviderName].StringVal;
        }
        if (propertyCount > EvtSystemEventID)
        {
            summary.EventId = values[EvtSystemEventID].UInt16Val;
        }
        if (propertyCount > EvtSystemLevel)
        {
            summary.Level = values[EvtSystemLevel].ByteVal;
        }
        if (propertyCount > EvtSystemEventRecordId)
        {
            summary.RecordId = values[EvtSystemEventRecordId].UInt64Val;
        }
        if (propertyCount > EvtSystemChannel && values[EvtSystemChannel].StringVal)
        {
            summary.Channel = values[EvtSystemChannel].StringVal;
        }
        return summary;
    }
}

namespace AstralChronicle::services
{
    std::vector<models::EventRecordSummary> WindowsEventQueryService::QueryRecent(
        std::wstring_view channel,
        std::uint32_t maximumRecords) const
    {
        if (channel.empty() || maximumRecords == 0)
        {
            return {};
        }

        std::wstring const channelPath{ channel };
        EventHandle const query{ EvtQuery(nullptr, channelPath.c_str(), L"*", EvtQueryChannelPath | EvtQueryReverseDirection) };
        if (!query.Get())
        {
            return {};
        }

        auto const batchSize = std::min<std::uint32_t>(maximumRecords, 64);
        std::vector<EVT_HANDLE> events(batchSize);
        DWORD returned{};
        if (!EvtNext(query.Get(), batchSize, events.data(), 0, 0, &returned))
        {
            return {};
        }

        std::vector<models::EventRecordSummary> results;
        results.reserve(returned);
        for (DWORD index{}; index < returned; ++index)
        {
            EventHandle const event{ events[index] };
            auto summary = RenderSummary(event.Get());
            if (!summary.Provider.empty())
            {
                results.emplace_back(std::move(summary));
            }
        }

        return results;
    }
}
