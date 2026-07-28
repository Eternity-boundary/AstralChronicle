// Created by EternityBoundary on Jul 29, 2026
#pragma once

#include <cstdint>
#include <string>

#include <winrt/base.h>

namespace AstralChronicle::services::details
{
    [[nodiscard]] inline std::wstring EncodeEventBookmarkPart(winrt::hstring const& value)
    {
        constexpr wchar_t Digits[] = L"0123456789ABCDEF";
        std::wstring result;
        result.reserve(value.size() * 4);
        for (auto const character : value)
        {
            auto const codeUnit = static_cast<std::uint16_t>(character);
            result.push_back(Digits[(codeUnit >> 12) & 0x0f]);
            result.push_back(Digits[(codeUnit >> 8) & 0x0f]);
            result.push_back(Digits[(codeUnit >> 4) & 0x0f]);
            result.push_back(Digits[codeUnit & 0x0f]);
        }
        return result;
    }

    [[nodiscard]] inline std::wstring EventBookmarkKey(
        std::uint64_t const recordId,
        std::int64_t const timestamp,
        winrt::hstring const& provider,
        winrt::hstring const& eventId,
        winrt::hstring const& channel)
    {
        return L"v2|" + std::to_wstring(recordId) + L"|" + std::to_wstring(timestamp) + L"|" +
            EncodeEventBookmarkPart(provider) + L"|" + EncodeEventBookmarkPart(eventId) + L"|" +
            EncodeEventBookmarkPart(channel);
    }

    [[nodiscard]] inline std::wstring LegacyEventBookmarkKey(
        winrt::hstring const& channel,
        winrt::hstring const& recordId)
    {
        return std::wstring{ channel.c_str() } + L"|" + std::wstring{ recordId.c_str() };
    }
}
