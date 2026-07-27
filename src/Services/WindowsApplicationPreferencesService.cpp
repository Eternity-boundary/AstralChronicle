// Created by EternityBoundary on Jul 28, 2026
#include "pch.h"
#include "WindowsApplicationPreferencesService.h"

#include "BookmarkPersistence.h"
#include "SessionPersistence.h"

#include <winrt/Windows.Storage.h>

#include <limits>

namespace
{
    [[nodiscard]] winrt::Windows::Foundation::IInspectable ReadSetting(
        std::wstring_view const key)
    {
        if (key.empty())
        {
            return nullptr;
        }

        auto const values = winrt::Windows::Storage::ApplicationData::Current()
            .LocalSettings()
            .Values();
        auto const settingKey = winrt::hstring{ key };
        return values.HasKey(settingKey) ? values.Lookup(settingKey) : nullptr;
    }
}

namespace AstralChronicle::services
{
    bool WindowsApplicationPreferencesService::ReadBool(
        std::wstring_view const key,
        bool const fallback) const noexcept
    {
        try
        {
            auto const boxed = ReadSetting(key);
            return boxed ? winrt::unbox_value<bool>(boxed) : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::uint32_t WindowsApplicationPreferencesService::ReadUInt32(
        std::wstring_view const key,
        std::uint32_t const fallback,
        std::uint32_t const minimum,
        std::uint32_t const maximum) const noexcept
    {
        if (minimum > maximum || fallback < minimum || fallback > maximum)
        {
            return fallback;
        }

        try
        {
            auto const boxed = ReadSetting(key);
            if (!boxed)
            {
                return fallback;
            }

            auto const text = winrt::unbox_value<winrt::hstring>(boxed);
            if (text.empty() || text.size() > 10)
            {
                return fallback;
            }

            std::uint64_t value{};
            for (auto const character : text)
            {
                if (character < L'0' || character > L'9')
                {
                    return fallback;
                }
                auto const digit = static_cast<std::uint32_t>(character - L'0');
                if (value > ((std::numeric_limits<std::uint32_t>::max)() - digit) / 10)
                {
                    return fallback;
                }
                value = value * 10 + digit;
            }
            return value >= minimum && value <= maximum
                ? static_cast<std::uint32_t>(value)
                : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    void WindowsApplicationPreferencesService::WriteBool(
        std::wstring_view const key,
        bool const value) const noexcept
    {
        try
        {
            winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values()
                .Insert(winrt::hstring{ key }, winrt::box_value(value));
        }
        catch (...)
        {
        }
    }

    void WindowsApplicationPreferencesService::WriteText(
        std::wstring_view const key,
        std::wstring_view const value) const noexcept
    {
        try
        {
            winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values()
                .Insert(winrt::hstring{ key }, winrt::box_value(winrt::hstring{ value }));
        }
        catch (...)
        {
        }
    }

    void WindowsApplicationPreferencesService::ClearPersistedSessions() const noexcept
    {
        (void)details::ClearPersistedDiagnosticSessions();
    }

    void WindowsApplicationPreferencesService::ClearPersistedBookmarks() const noexcept
    {
        (void)details::ClearPersistedEventLogBookmarks();
    }
}
