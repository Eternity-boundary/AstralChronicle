// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "EventLogItemViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "EventLogItemViewModel.g.cpp"

#include <winrt/Windows.Globalization.DateTimeFormatting.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <ratio>

namespace
{
    constexpr std::int64_t WindowsEpochOffset100Nanoseconds = 116444736000000000LL;

    [[nodiscard]] winrt::hstring ValueOrFallback(
        std::wstring const& value,
        winrt::hstring const& fallback)
    {
        return value.empty() ? fallback : winrt::hstring{ value };
    }

    [[nodiscard]] winrt::hstring LevelResourceKey(std::uint8_t const level)
    {
        switch (level)
        {
        case 1:
            return L"EventLogs.Level.Critical";
        case 2:
            return L"EventLogs.Level.Error";
        case 3:
            return L"EventLogs.Level.Warning";
        case 4:
            return L"EventLogs.Level.Information";
        case 5:
            return L"EventLogs.Level.Verbose";
        default:
            return L"EventLogs.Level.Unknown";
        }
    }

    [[nodiscard]] winrt::hstring FormatTime(
        std::chrono::system_clock::time_point const timeCreated,
        winrt::hstring const& fallback)
    {
        if (timeCreated.time_since_epoch().count() == 0)
        {
            return fallback;
        }

        using WindowsDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
        auto const unixTicks = std::chrono::duration_cast<WindowsDuration>(
            timeCreated.time_since_epoch()).count();
        if (unixTicks < -WindowsEpochOffset100Nanoseconds ||
            unixTicks > ((std::numeric_limits<std::int64_t>::max)() - WindowsEpochOffset100Nanoseconds))
        {
            return fallback;
        }

        try
        {
            winrt::Windows::Foundation::DateTime const dateTime{
                winrt::clock::duration{ unixTicks + WindowsEpochOffset100Nanoseconds } };
            winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter formatter{
                L"shortdate shorttime" };
            return formatter.Format(dateTime);
        }
        catch (...)
        {
            return fallback;
        }
    }
}

namespace winrt::AstralChronicle::implementation
{
    EventLogItemViewModel::EventLogItemViewModel()
    {
    }

    void EventLogItemViewModel::Initialize(
        ::AstralChronicle::models::EventRecordSummary const& summary,
        ::AstralChronicle::design::IStringResourceService const& strings)
    {
        auto const emptyValue = strings.GetString(L"EventLogs.EmptyValue.Text");
        m_timeCreated = FormatTime(summary.TimeCreated, emptyValue);
        m_level = strings.GetString(LevelResourceKey(summary.Level));
        m_provider = ValueOrFallback(summary.Provider, emptyValue);
        m_eventId = summary.EventId == 0 ? emptyValue : winrt::to_hstring(summary.EventId);
        m_version = winrt::to_hstring(summary.Version);
        m_taskCategory = ValueOrFallback(summary.TaskDisplayName, emptyValue);
        m_opcode = winrt::to_hstring(summary.Opcode);
        m_keywords = winrt::to_hstring(summary.Keywords);
        m_channel = ValueOrFallback(summary.Channel, emptyValue);
        m_user = ValueOrFallback(summary.User, emptyValue);
        m_computer = ValueOrFallback(summary.Computer, emptyValue);
        m_shortDescription = ValueOrFallback(
            summary.ShortDescription,
            strings.GetString(L"EventLogs.MessagePreviewUnavailable.Text"));
        m_recordId = summary.RecordId == 0 ? emptyValue : winrt::to_hstring(summary.RecordId);
        m_processId = summary.ProcessId == 0 ? emptyValue : winrt::to_hstring(summary.ProcessId);
        m_threadId = summary.ThreadId == 0 ? emptyValue : winrt::to_hstring(summary.ThreadId);
        m_activityId = ValueOrFallback(summary.ActivityId, emptyValue);
        m_relatedActivityId = ValueOrFallback(summary.RelatedActivityId, emptyValue);
        m_isBookmarked = false;
        m_sortTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            summary.TimeCreated.time_since_epoch()).count();
        m_sortRecordId = summary.RecordId;
    }

    winrt::hstring EventLogItemViewModel::TimeCreated() const { return m_timeCreated; }
    winrt::hstring EventLogItemViewModel::Level() const { return m_level; }
    winrt::hstring EventLogItemViewModel::Provider() const { return m_provider; }
    winrt::hstring EventLogItemViewModel::EventId() const { return m_eventId; }
    winrt::hstring EventLogItemViewModel::Version() const { return m_version; }
    winrt::hstring EventLogItemViewModel::TaskCategory() const { return m_taskCategory; }
    winrt::hstring EventLogItemViewModel::Opcode() const { return m_opcode; }
    winrt::hstring EventLogItemViewModel::Keywords() const { return m_keywords; }
    winrt::hstring EventLogItemViewModel::Channel() const { return m_channel; }
    winrt::hstring EventLogItemViewModel::User() const { return m_user; }
    winrt::hstring EventLogItemViewModel::Computer() const { return m_computer; }
    winrt::hstring EventLogItemViewModel::ShortDescription() const { return m_shortDescription; }
    winrt::hstring EventLogItemViewModel::RecordId() const { return m_recordId; }
    winrt::hstring EventLogItemViewModel::ProcessId() const { return m_processId; }
    winrt::hstring EventLogItemViewModel::ThreadId() const { return m_threadId; }
    winrt::hstring EventLogItemViewModel::ActivityId() const { return m_activityId; }
    winrt::hstring EventLogItemViewModel::RelatedActivityId() const { return m_relatedActivityId; }
    bool EventLogItemViewModel::IsBookmarked() const noexcept { return m_isBookmarked; }
    void EventLogItemViewModel::IsBookmarked(bool const value)
    {
        if (m_isBookmarked == value)
        {
            return;
        }
        m_isBookmarked = value;
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsBookmarked" });
    }
    std::int64_t EventLogItemViewModel::SortTimestamp() const noexcept { return m_sortTimestamp; }
    std::uint64_t EventLogItemViewModel::SortRecordId() const noexcept { return m_sortRecordId; }

    winrt::event_token EventLogItemViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void EventLogItemViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }
}
