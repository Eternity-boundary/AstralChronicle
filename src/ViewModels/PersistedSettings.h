// Created by OpenAI Codex on Jul 23, 2026
#pragma once

#include <winrt/Windows.Storage.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace AstralChronicle::viewmodels
{
    class PersistedSettingsReader final
    {
    public:
        PersistedSettingsReader() noexcept
        {
            try
            {
                m_values = winrt::Windows::Storage::ApplicationData::Current()
                    .LocalSettings()
                    .Values();
            }
            catch (...)
            {
                m_values = nullptr;
            }
        }

        [[nodiscard]] bool ReadBool(
            std::wstring_view const key,
            bool const fallback) const noexcept
        {
            try
            {
                auto const boxed = Lookup(key);
                return boxed ? winrt::unbox_value<bool>(boxed) : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        [[nodiscard]] std::wstring ReadString(
            std::wstring_view const key,
            std::wstring_view const fallback,
            std::size_t const minimumLength,
            std::size_t const maximumLength) const noexcept
        {
            try
            {
                auto const boxed = Lookup(key);
                if (!boxed) return std::wstring{ fallback };
                auto const value = winrt::unbox_value<winrt::hstring>(boxed);
                if (value.size() < minimumLength || value.size() > maximumLength)
                {
                    return std::wstring{ fallback };
                }
                std::wstring result{ value.c_str(), value.size() };
                if (result.find(L'\0') != std::wstring::npos)
                {
                    return std::wstring{ fallback };
                }
                return result;
            }
            catch (...)
            {
                return std::wstring{ fallback };
            }
        }

        [[nodiscard]] std::uint32_t ReadUInt32(
            std::wstring_view const key,
            std::uint32_t const fallback,
            std::uint32_t const minimum,
            std::uint32_t const maximum) const noexcept
        {
            if (minimum > maximum || fallback < minimum || fallback > maximum)
            {
                return fallback;
            }

            auto const text = ReadString(key, {}, 1, 10);
            if (text.empty()) return fallback;
            std::uint64_t value{};
            for (auto const character : text)
            {
                if (character < L'0' || character > L'9') return fallback;
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

    private:
        [[nodiscard]] winrt::Windows::Foundation::IInspectable Lookup(
            std::wstring_view const key) const
        {
            if (!m_values || key.empty()) return nullptr;
            auto const settingKey = winrt::hstring{ key };
            return m_values.HasKey(settingKey) ? m_values.Lookup(settingKey) : nullptr;
        }

        winrt::Windows::Foundation::Collections::IPropertySet m_values{ nullptr };
    };

    struct EventItemSettings final
    {
        bool UseUtc{};
        bool RedactComputerName{};
        bool RedactUserNames{};
    };

    struct PersistedSettingsSnapshot final
    {
        bool DefaultSortDescending{ true };
        bool GroupRepeatedEvents{};
        bool PersistBookmarks{ true };
        std::uint32_t LiveQueueLimit{ 5'000 };
        std::uint32_t QueryBatchSize{ 64 };
        std::uint32_t MaxVisibleLiveRows{ 2'000 };
        EventItemSettings EventItems;

        [[nodiscard]] static PersistedSettingsSnapshot Load() noexcept
        {
            PersistedSettingsReader const reader;
            PersistedSettingsSnapshot result;
            result.DefaultSortDescending = reader.ReadBool(
                L"EventDisplay.DefaultSortDescending",
                result.DefaultSortDescending);
            result.GroupRepeatedEvents = reader.ReadBool(
                L"EventDisplay.GroupRepeatedEvents",
                result.GroupRepeatedEvents);
            result.PersistBookmarks = reader.ReadBool(
                L"Storage.PersistBookmarks",
                result.PersistBookmarks);
            result.LiveQueueLimit = reader.ReadUInt32(
                L"Monitoring.LiveQueueLimit",
                result.LiveQueueLimit,
                64,
                100'000);
            result.QueryBatchSize = reader.ReadUInt32(
                L"Performance.QueryBatchSize",
                result.QueryBatchSize,
                1,
                4'096);
            result.MaxVisibleLiveRows = reader.ReadUInt32(
                L"Performance.MaxVisibleLiveRows",
                result.MaxVisibleLiveRows,
                64,
                100'000);
            result.EventItems.UseUtc = reader.ReadBool(L"EventDisplay.UseUtc", false);
            result.EventItems.RedactComputerName = reader.ReadBool(
                L"Privacy.RedactComputerName",
                false);
            result.EventItems.RedactUserNames = reader.ReadBool(
                L"Privacy.RedactUserNames",
                false);
            return result;
        }
    };
}
