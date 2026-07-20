// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "EventLogsViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "EventLogItemViewModel.h"
#include "Services/EventQueryBuilder.h"

#include "EventLogsViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <winrt/Windows.Storage.h>

#include <string>
#include <vector>
#include <utility>
#include <cwchar>
#include <cwctype>
#include <algorithm>
#include <string_view>
#include <sstream>

namespace
{
    [[nodiscard]] winrt::hstring FormatResource(
        winrt::hstring format,
        std::vector<winrt::hstring> const& values)
    {
        auto result = std::wstring{ format.c_str() };
        for (std::size_t index{}; index < values.size(); ++index)
        {
            auto const token = std::wstring{ L"{" } + std::to_wstring(index) + L"}";
            auto const position = result.find(token);
            if (position != std::wstring::npos)
            {
                result.replace(position, token.size(), values[index].c_str());
            }
        }
        return winrt::hstring{ result };
    }

    [[nodiscard]] winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity SeverityFor(
        AstralChronicle::services::EventQueryStatus const status)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        switch (status)
        {
        case Status::AccessDenied:
        case Status::InvalidChannel:
        case Status::ServiceUnavailable:
        case Status::Failed:
            return winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error;
        case Status::Cancelled:
        case Status::NoEvents:
        case Status::Succeeded:
        default:
            return winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        }
    }

    [[nodiscard]] std::wstring Lowercase(std::wstring value)
    {
        for (auto& character : value)
        {
            character = static_cast<wchar_t>(std::towlower(character));
        }
        return value;
    }

    [[nodiscard]] bool Contains(
        winrt::AstralChronicle::EventLogItemViewModel const& item,
        std::wstring const& needle)
    {
        auto const matches = [&needle](winrt::hstring const& value)
        {
            return Lowercase(std::wstring{ value.c_str() }).find(needle) != std::wstring::npos;
        };
        return matches(item.TimeCreated()) ||
            matches(item.Level()) ||
            matches(item.Provider()) ||
            matches(item.EventId()) ||
            matches(item.TaskCategory()) ||
            matches(item.Channel()) ||
            matches(item.User()) ||
            matches(item.Computer()) ||
            matches(item.ShortDescription()) ||
            matches(item.RecordId());
    }

    [[nodiscard]] std::optional<std::uint64_t> ExtractRecordId(
        std::optional<std::wstring> const& query)
    {
        if (!query)
        {
            return std::nullopt;
        }

        constexpr std::wstring_view marker = L"EventRecordID=";
        auto const markerPosition = query->find(marker);
        if (markerPosition == std::wstring::npos)
        {
            return std::nullopt;
        }

        auto const valueStart = markerPosition + marker.size();
        auto const valueEnd = query->find_first_not_of(L"0123456789", valueStart);
        auto const value = query->substr(valueStart, valueEnd == std::wstring::npos
            ? std::wstring::npos
            : valueEnd - valueStart);
        if (value.empty())
        {
            return std::nullopt;
        }

        try
        {
            return std::stoull(value);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::wstring BookmarkKey(
        winrt::AstralChronicle::EventLogItemViewModel const& value)
    {
        return std::wstring{ value.Channel().c_str() } + L"|" +
            std::wstring{ value.RecordId().c_str() };
    }

    [[nodiscard]] std::wstring FormatProperties(
        std::vector<::AstralChronicle::models::EventProperty> const& properties)
    {
        std::wstring result;
        for (auto const& property : properties)
        {
            if (!result.empty())
            {
                result += L"\r\n";
            }
            result += property.Name;
            if (!property.Value.empty())
            {
                result += L"\t";
                result += property.Value;
            }
        }
        return result;
    }

    [[nodiscard]] std::wstring FormatProviderMetadata(
        ::AstralChronicle::models::ProviderMetadataSnapshot const& metadata)
    {
        std::wstring result;
        auto append = [&result](std::wstring const& value)
        {
            if (!value.empty())
            {
                if (!result.empty()) result += L"\r\n";
                result += value;
            }
        };
        append(metadata.Name);
        append(metadata.Guid);
        append(metadata.ResourceFilePath);
        append(metadata.MessageFilePath);
        append(metadata.HelpLink);
        return result;
    }
}

namespace winrt::AstralChronicle::implementation
{
    EventLogsViewModel::EventLogsViewModel()
        : m_events(winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>())
    {
    }

    void EventLogsViewModel::Initialize(
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
        std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel,
        std::optional<std::wstring> const& query)
    {
        m_eventQuery = &eventQuery;
        m_strings = &strings;
        m_dispatcher = dispatcher;
        m_heading = strings.GetString(L"EventLogs.Heading");
        auto const defaultChannel = strings.GetString(L"EventLogs.ChannelDefault.Text");
        m_channelPath = channel && !channel->Path.empty()
            ? channel->Path
            : std::wstring{ defaultChannel.c_str(), defaultChannel.size() };
        m_baseQuery = query && !query->empty() ? *query : L"*";
        m_query = m_baseQuery;
        m_initialRecordId = ExtractRecordId(query);
        m_statusText = strings.GetString(L"EventLogs.Loading.Text");
        m_statusDetails = strings.GetString(L"EventLogs.LoadingDetails.Text");
        m_searchText.clear();
        m_filterProvider.clear();
        m_filterEventId.clear();
        m_filterLevel = L"Any";
        m_filterTask.clear();
        m_filterOpcode.clear();
        m_filterKeyword.clear();
        m_filterProcessId.clear();
        m_filterThreadId.clear();
        m_filterUser.clear();
        m_filterComputer.clear();
        m_rawXPath.clear();
        m_filterAfterToday = false;
        m_filterAfterHours.clear();
        m_hasStructuredFilter = false;
        LoadBookmarks();
        m_filterSummary = strings.GetString(L"EventLogs.FilterNone.Text");
        ClearSelection();
        RaisePropertyChanged(L"Heading");
        RaisePropertyChanged(L"ChannelPath");
        Refresh();
    }

    winrt::hstring EventLogsViewModel::Heading() const { return m_heading; }

    winrt::hstring EventLogsViewModel::ChannelPath() const
    {
        return winrt::hstring{ m_channelPath };
    }

    winrt::hstring EventLogsViewModel::StatusText() const { return m_statusText; }
    winrt::hstring EventLogsViewModel::StatusDetails() const { return m_statusDetails; }
    bool EventLogsViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    bool EventLogsViewModel::IsAccessDenied() const noexcept { return m_isAccessDenied; }
    bool EventLogsViewModel::IsLoading() const noexcept { return m_isLoading; }
    winrt::hstring EventLogsViewModel::SearchText() const { return m_searchText; }
    void EventLogsViewModel::SearchText(winrt::hstring const& value)
    {
        if (m_searchText == value)
        {
            return;
        }

        m_searchText = value;
        ApplyFilter();
        RaisePropertyChanged(L"SearchText");
    }
    winrt::hstring EventLogsViewModel::FilterProvider() const { return m_filterProvider; }
    void EventLogsViewModel::FilterProvider(winrt::hstring const& value)
    {
        if (m_filterProvider == value)
        {
            return;
        }
        m_filterProvider = value;
        RaisePropertyChanged(L"FilterProvider");
    }
    winrt::hstring EventLogsViewModel::FilterEventId() const { return m_filterEventId; }
    void EventLogsViewModel::FilterEventId(winrt::hstring const& value)
    {
        if (m_filterEventId == value)
        {
            return;
        }
        m_filterEventId = value;
        RaisePropertyChanged(L"FilterEventId");
    }
    winrt::hstring EventLogsViewModel::FilterLevel() const { return m_filterLevel; }
    void EventLogsViewModel::FilterLevel(winrt::hstring const& value)
    {
        if (m_filterLevel == value)
        {
            return;
        }
        m_filterLevel = value;
        RaisePropertyChanged(L"FilterLevel");
    }
    winrt::hstring EventLogsViewModel::FilterTask() const { return m_filterTask; }
    void EventLogsViewModel::FilterTask(winrt::hstring const& value)
    {
        if (m_filterTask == value)
        {
            return;
        }
        m_filterTask = value;
        RaisePropertyChanged(L"FilterTask");
    }
    winrt::hstring EventLogsViewModel::FilterOpcode() const { return m_filterOpcode; }
    void EventLogsViewModel::FilterOpcode(winrt::hstring const& value)
    {
        if (m_filterOpcode == value) return;
        m_filterOpcode = value;
        RaisePropertyChanged(L"FilterOpcode");
    }
    winrt::hstring EventLogsViewModel::FilterKeyword() const { return m_filterKeyword; }
    void EventLogsViewModel::FilterKeyword(winrt::hstring const& value)
    {
        if (m_filterKeyword == value) return;
        m_filterKeyword = value;
        RaisePropertyChanged(L"FilterKeyword");
    }
    winrt::hstring EventLogsViewModel::FilterProcessId() const { return m_filterProcessId; }
    void EventLogsViewModel::FilterProcessId(winrt::hstring const& value)
    {
        if (m_filterProcessId == value) return;
        m_filterProcessId = value;
        RaisePropertyChanged(L"FilterProcessId");
    }
    winrt::hstring EventLogsViewModel::FilterThreadId() const { return m_filterThreadId; }
    void EventLogsViewModel::FilterThreadId(winrt::hstring const& value)
    {
        if (m_filterThreadId == value) return;
        m_filterThreadId = value;
        RaisePropertyChanged(L"FilterThreadId");
    }
    winrt::hstring EventLogsViewModel::FilterUser() const { return m_filterUser; }
    void EventLogsViewModel::FilterUser(winrt::hstring const& value)
    {
        if (m_filterUser == value)
        {
            return;
        }
        m_filterUser = value;
        RaisePropertyChanged(L"FilterUser");
    }
    winrt::hstring EventLogsViewModel::FilterComputer() const { return m_filterComputer; }
    void EventLogsViewModel::FilterComputer(winrt::hstring const& value)
    {
        if (m_filterComputer == value)
        {
            return;
        }
        m_filterComputer = value;
        RaisePropertyChanged(L"FilterComputer");
    }
    winrt::hstring EventLogsViewModel::RawXPath() const { return m_rawXPath; }
    void EventLogsViewModel::RawXPath(winrt::hstring const& value)
    {
        if (m_rawXPath == value)
        {
            return;
        }
        m_rawXPath = value;
        RaisePropertyChanged(L"RawXPath");
    }
    bool EventLogsViewModel::FilterAfterToday() const noexcept { return m_filterAfterToday; }
    void EventLogsViewModel::FilterAfterToday(bool const value)
    {
        if (m_filterAfterToday == value)
        {
            return;
        }
        m_filterAfterToday = value;
        RaisePropertyChanged(L"FilterAfterToday");
    }
    winrt::hstring EventLogsViewModel::FilterAfterHours() const { return m_filterAfterHours; }
    void EventLogsViewModel::FilterAfterHours(winrt::hstring const& value)
    {
        if (m_filterAfterHours == value) return;
        m_filterAfterHours = value;
        RaisePropertyChanged(L"FilterAfterHours");
    }
    bool EventLogsViewModel::HasStructuredFilter() const noexcept { return m_hasStructuredFilter; }
    winrt::hstring EventLogsViewModel::FilterSummary() const { return m_filterSummary; }
    bool EventLogsViewModel::HasFilter() const noexcept
    {
        return !m_searchText.empty() || m_hasStructuredFilter;
    }
    bool EventLogsViewModel::HasSelection() const noexcept
    {
        return !m_selectedEvents.empty() || static_cast<bool>(m_selectedEvent);
    }
    std::uint32_t EventLogsViewModel::SelectedCount() const noexcept
    {
        return static_cast<std::uint32_t>(
            m_selectedEvents.empty() ? (m_selectedEvent ? 1u : 0u) : m_selectedEvents.size());
    }
    winrt::hstring EventLogsViewModel::CopySelectedEventText() const
    {
        return CopySelectedEventsText();
    }
    winrt::hstring EventLogsViewModel::CopySelectedEventsText() const
    {
        auto const values = m_selectedEvents.empty()
            ? std::vector<winrt::AstralChronicle::EventLogItemViewModel>{ m_selectedEvent }
            : m_selectedEvents;
        if (values.empty() || !values.front())
        {
            return {};
        }

        std::wstring text;
        auto append = [&text](winrt::hstring const& value)
        {
            if (!text.empty())
            {
                text += L"\t";
            }
            text += value.c_str();
        };
        bool firstRow = true;
        for (auto const& value : values)
        {
            if (!value)
            {
                continue;
            }
            if (!firstRow)
            {
                text += L"\r\n";
            }
            firstRow = false;
            append(value.TimeCreated());
            append(value.Level());
            append(value.Provider());
            append(value.EventId());
            append(value.TaskCategory());
            append(value.Channel());
            append(value.User());
            append(value.Computer());
            append(value.ShortDescription());
        }
        return winrt::hstring{ text };
    }
    void EventLogsViewModel::ToggleBookmarks()
    {
        auto const values = m_selectedEvents.empty()
            ? std::vector<winrt::AstralChronicle::EventLogItemViewModel>{ m_selectedEvent }
            : m_selectedEvents;
        if (values.empty() || !values.front())
        {
            return;
        }

        auto const shouldBookmark = std::any_of(
            values.begin(),
            values.end(),
            [](winrt::AstralChronicle::EventLogItemViewModel const& value)
            {
                return !value.IsBookmarked();
            });
        for (auto const& value : values)
        {
            if (value)
            {
                value.IsBookmarked(shouldBookmark);
                if (shouldBookmark)
                {
                    m_bookmarkedKeys.insert(BookmarkKey(value));
                }
                else
                {
                    m_bookmarkedKeys.erase(BookmarkKey(value));
                }
            }
        }
        PersistBookmarks();
    }

    void EventLogsViewModel::ExportSelectedEvents()
    {
        auto const text = CopySelectedEventsText();
        if (text.empty() || !m_dispatcher || !m_strings)
        {
            return;
        }

        m_hasStatusMessage = true;
        m_isAccessDenied = false;
        m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        m_statusText = m_strings->GetString(L"EventLogs.ExportLoading.Text");
        m_statusDetails.clear();
        RaiseStatusProperties();
        ExportSelectedEventsAsync(
            std::wstring{ text.c_str(), text.size() },
            m_requestVersion);
    }

    winrt::fire_and_forget EventLogsViewModel::ExportSelectedEventsAsync(
        std::wstring text,
        std::uint64_t const requestVersion)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        auto strings = m_strings;
        co_await winrt::resume_background();

        winrt::hstring path;
        std::uint32_t errorCode{};
        try
        {
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = co_await folder.CreateFileAsync(
                L"EventLogs-export.txt",
                winrt::Windows::Storage::CreationCollisionOption::GenerateUniqueName);
            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, text);
            path = file.Path();
        }
        catch (winrt::hresult_error const& error)
        {
            errorCode = static_cast<std::uint32_t>(error.code().value);
        }

        co_await wil::resume_foreground(dispatcher);
        if (requestVersion != m_requestVersion)
        {
            co_return;
        }

        if (errorCode == 0)
        {
            m_statusText = strings->GetString(L"EventLogs.ExportCompleted.Text");
            m_statusDetails = FormatResource(
                strings->GetString(L"EventLogs.ExportCompletedDetails.Text"),
                { path });
            m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success;
        }
        else
        {
            m_statusText = strings->GetString(L"EventLogs.ExportFailed.Text");
            m_statusDetails = FormatResource(
                strings->GetString(L"EventLogs.ErrorDetails.Text"),
                { winrt::to_hstring(errorCode) });
            m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error;
        }
        m_hasStatusMessage = true;
        RaiseStatusProperties();
    }
    winrt::hstring EventLogsViewModel::SortKey() const { return m_sortKey; }
    bool EventLogsViewModel::SortAscending() const noexcept { return m_sortAscending; }
    Microsoft::UI::Xaml::Controls::InfoBarSeverity EventLogsViewModel::StatusSeverity() const noexcept { return m_statusSeverity; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> EventLogsViewModel::Events() const
    {
        return m_events;
    }

    winrt::AstralChronicle::EventLogItemViewModel EventLogsViewModel::SelectedEvent() const
    {
        return m_selectedEvent;
    }

    void EventLogsViewModel::SelectedEvent(winrt::AstralChronicle::EventLogItemViewModel const& value)
    {
        if (m_selectedEvent == value)
        {
            return;
        }

        m_selectedEvent = value;
        if (m_selectedEvent)
        {
            if (m_selectedEvents.empty())
            {
                m_selectedEvents.emplace_back(m_selectedEvent);
            }
            m_selectedProvider = m_selectedEvent.Provider();
            m_selectedEventId = m_selectedEvent.EventId();
            m_selectedVersion = m_selectedEvent.Version();
            m_selectedLevel = m_selectedEvent.Level();
            m_selectedOpcode = m_selectedEvent.Opcode();
            m_selectedKeywords = m_selectedEvent.Keywords();
            m_selectedTimeCreated = m_selectedEvent.TimeCreated();
            m_selectedTaskCategory = m_selectedEvent.TaskCategory();
            m_selectedChannel = m_selectedEvent.Channel();
            m_selectedUser = m_selectedEvent.User();
            m_selectedComputer = m_selectedEvent.Computer();
            m_selectedRecordId = m_selectedEvent.RecordId();
            m_selectedProcessId = m_selectedEvent.ProcessId();
            m_selectedThreadId = m_selectedEvent.ThreadId();
            m_selectedActivityId = m_selectedEvent.ActivityId();
            m_selectedRelatedActivityId = m_selectedEvent.RelatedActivityId();
            m_selectedDescription = m_selectedEvent.ShortDescription();
            m_detailsStatusText = m_strings->GetString(L"EventLogs.DetailsLoading.Text");
            m_selectedMessage = m_strings->GetString(L"EventLogs.DetailsLoading.Text");
            m_selectedXml = m_strings->GetString(L"EventLogs.DetailsLoading.Text");
            m_isDetailsLoading = true;

            if (m_detailsCancellation)
            {
                m_detailsCancellation->store(true, std::memory_order_relaxed);
            }
            m_detailsCancellation = ::AstralChronicle::services::MakeQueryCancellation();
            auto const recordId = std::wcstoull(m_selectedRecordId.c_str(), nullptr, 10);
            auto const requestVersion = ++m_detailsRequestVersion;
            auto selectedChannel = std::wstring{ m_selectedEvent.Channel().c_str() };
            if (selectedChannel.empty() || selectedChannel == m_strings->GetString(L"EventLogs.EmptyValue.Text"))
            {
                selectedChannel = m_channelPath;
            }
            LoadDetailsAsync(
                requestVersion,
                std::move(selectedChannel),
                recordId,
                m_detailsCancellation);
        }
        else
        {
            if (m_detailsCancellation)
            {
                m_detailsCancellation->store(true, std::memory_order_relaxed);
            }
            ++m_detailsRequestVersion;
            ClearSelection();
        }

        RaisePropertyChanged(L"SelectedEvent");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectedCount");
        RaiseSelectionProperties();
    }

    void EventLogsViewModel::SetSelectedEvents(
        std::vector<winrt::AstralChronicle::EventLogItemViewModel> const& values,
        winrt::AstralChronicle::EventLogItemViewModel const& preferred)
    {
        m_selectedEvents = values;
        if (m_selectedEvents.empty())
        {
            if (m_selectedEvent)
            {
                SelectedEvent(winrt::AstralChronicle::EventLogItemViewModel{ nullptr });
            }
            else
            {
                RaisePropertyChanged(L"HasSelection");
                RaisePropertyChanged(L"SelectedCount");
            }
            return;
        }

        auto active = preferred;
        if (!active || std::find(m_selectedEvents.begin(), m_selectedEvents.end(), active) == m_selectedEvents.end())
        {
            active = m_selectedEvents.back();
        }

        if (m_selectedEvent != active)
        {
            SelectedEvent(active);
        }
        else
        {
            RaisePropertyChanged(L"HasSelection");
            RaisePropertyChanged(L"SelectedCount");
        }
    }

    winrt::hstring EventLogsViewModel::SelectedProvider() const { return m_selectedProvider; }
    winrt::hstring EventLogsViewModel::SelectedEventId() const { return m_selectedEventId; }
    winrt::hstring EventLogsViewModel::SelectedVersion() const { return m_selectedVersion; }
    winrt::hstring EventLogsViewModel::SelectedLevel() const { return m_selectedLevel; }
    winrt::hstring EventLogsViewModel::SelectedOpcode() const { return m_selectedOpcode; }
    winrt::hstring EventLogsViewModel::SelectedKeywords() const { return m_selectedKeywords; }
    winrt::hstring EventLogsViewModel::SelectedTimeCreated() const { return m_selectedTimeCreated; }
    winrt::hstring EventLogsViewModel::SelectedTaskCategory() const { return m_selectedTaskCategory; }
    winrt::hstring EventLogsViewModel::SelectedChannel() const { return m_selectedChannel; }
    winrt::hstring EventLogsViewModel::SelectedUser() const { return m_selectedUser; }
    winrt::hstring EventLogsViewModel::SelectedComputer() const { return m_selectedComputer; }
    winrt::hstring EventLogsViewModel::SelectedRecordId() const { return m_selectedRecordId; }
    winrt::hstring EventLogsViewModel::SelectedProcessId() const { return m_selectedProcessId; }
    winrt::hstring EventLogsViewModel::SelectedThreadId() const { return m_selectedThreadId; }
    winrt::hstring EventLogsViewModel::SelectedActivityId() const { return m_selectedActivityId; }
    winrt::hstring EventLogsViewModel::SelectedRelatedActivityId() const { return m_selectedRelatedActivityId; }
    winrt::hstring EventLogsViewModel::SelectedDescription() const { return m_selectedDescription; }
    winrt::hstring EventLogsViewModel::SelectedMessage() const { return m_selectedMessage; }
    winrt::hstring EventLogsViewModel::SelectedXml() const { return m_selectedXml; }
    winrt::hstring EventLogsViewModel::SelectedEventData() const { return m_selectedEventData; }
    winrt::hstring EventLogsViewModel::SelectedUserData() const { return m_selectedUserData; }
    winrt::hstring EventLogsViewModel::SelectedProviderMetadata() const { return m_selectedProviderMetadata; }
    winrt::hstring EventLogsViewModel::SelectedBinaryData() const { return m_selectedBinaryData; }
    winrt::hstring EventLogsViewModel::SelectedRelatedEvents() const { return m_selectedRelatedEvents; }
    winrt::hstring EventLogsViewModel::DetailsStatusText() const { return m_detailsStatusText; }
    bool EventLogsViewModel::IsDetailsLoading() const noexcept { return m_isDetailsLoading; }

    void EventLogsViewModel::Refresh()
    {
        if (!m_eventQuery || !m_strings || !m_dispatcher)
        {
            return;
        }

        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        m_cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_requestVersion;
        auto const cancellation = m_cancellation;
        auto const channel = m_channelPath;

        m_isLoading = true;
        m_hasStatusMessage = true;
        m_isAccessDenied = false;
        m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        m_statusText = m_strings->GetString(L"EventLogs.Loading.Text");
        m_statusDetails = m_strings->GetString(L"EventLogs.LoadingDetails.Text");
        m_events = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_allEvents = m_events;
        m_filterSummary = m_strings->GetString(L"EventLogs.FilterNone.Text");
        ClearSelection();
        RaiseStatusProperties();
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"FilterSummary");
        RaisePropertyChanged(L"HasFilter");
        RaisePropertyChanged(L"HasSelection");
        RaiseSelectionProperties();
        RaisePropertyChanged(L"SelectedMessage");
        RaisePropertyChanged(L"SelectedXml");
        RaisePropertyChanged(L"DetailsStatusText");
        RaisePropertyChanged(L"IsDetailsLoading");
        LoadAsync(requestVersion, channel, cancellation);
    }

    void EventLogsViewModel::ClearFilter()
    {
        m_searchText.clear();
        m_filterProvider.clear();
        m_filterEventId.clear();
        m_filterLevel = L"Any";
        m_filterTask.clear();
        m_filterOpcode.clear();
        m_filterKeyword.clear();
        m_filterProcessId.clear();
        m_filterThreadId.clear();
        m_filterUser.clear();
        m_filterComputer.clear();
        m_rawXPath.clear();
        m_filterAfterToday = false;
        m_filterAfterHours.clear();
        m_hasStructuredFilter = false;
        m_query = m_baseQuery;
        RaiseFilterProperties();
        Refresh();
    }

    void EventLogsViewModel::ApplyStructuredFilter()
    {
        ::AstralChronicle::models::EventFilter filter;
        filter.Provider = std::wstring{ m_filterProvider.c_str() };
        filter.EventId = std::wstring{ m_filterEventId.c_str() };
        filter.Task = std::wstring{ m_filterTask.c_str() };
        filter.Opcode = std::wstring{ m_filterOpcode.c_str() };
        filter.Keyword = std::wstring{ m_filterKeyword.c_str() };
        filter.ProcessId = std::wstring{ m_filterProcessId.c_str() };
        filter.ThreadId = std::wstring{ m_filterThreadId.c_str() };
        filter.User = std::wstring{ m_filterUser.c_str() };
        filter.Computer = std::wstring{ m_filterComputer.c_str() };
        filter.RawXPath = std::wstring{ m_rawXPath.c_str() };
        filter.AfterToday = m_filterAfterToday;
        try
        {
            filter.AfterHours = m_filterAfterHours.empty()
                ? 0u
                : static_cast<std::uint32_t>(std::stoul(std::wstring{ m_filterAfterHours.c_str() }));
        }
        catch (...)
        {
            filter.AfterHours = 0;
        }

        if (m_filterLevel != L"Any")
        {
            if (m_filterLevel == L"Critical") filter.Level = L"1";
            else if (m_filterLevel == L"Error") filter.Level = L"2";
            else if (m_filterLevel == L"Warning") filter.Level = L"3";
            else if (m_filterLevel == L"Information") filter.Level = L"4";
        }

        m_hasStructuredFilter = filter.HasStructuredFilter();
        m_query = m_hasStructuredFilter
            ? ::AstralChronicle::services::BuildEventQuery(filter)
            : m_baseQuery;
        RaiseFilterProperties();
        Refresh();
    }

    void EventLogsViewModel::ClearStructuredFilter()
    {
        m_filterProvider.clear();
        m_filterEventId.clear();
        m_filterLevel = L"Any";
        m_filterTask.clear();
        m_filterOpcode.clear();
        m_filterKeyword.clear();
        m_filterProcessId.clear();
        m_filterThreadId.clear();
        m_filterUser.clear();
        m_filterComputer.clear();
        m_rawXPath.clear();
        m_filterAfterToday = false;
        m_filterAfterHours.clear();
        m_hasStructuredFilter = false;
        m_query = m_baseQuery;
        RaiseFilterProperties();
        Refresh();
    }

    void EventLogsViewModel::SortBy(winrt::hstring const& key)
    {
        if (key.empty())
        {
            return;
        }

        if (m_sortKey == key)
        {
            m_sortAscending = !m_sortAscending;
        }
        else
        {
            m_sortKey = key;
            m_sortAscending = true;
        }
        ApplyFilter();
        RaisePropertyChanged(L"SortKey");
        RaisePropertyChanged(L"SortAscending");
    }

    winrt::fire_and_forget EventLogsViewModel::LoadDetailsAsync(
        std::uint64_t const requestVersion,
        std::wstring channel,
        std::uint64_t const recordId,
        ::AstralChronicle::services::QueryCancellation cancellation)
    {
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        auto const result = m_eventQuery->QueryDetails(channel, recordId, cancellation);
        co_await wil::resume_foreground(m_dispatcher);
        if (requestVersion != m_detailsRequestVersion || cancellation != m_detailsCancellation)
        {
            co_return;
        }

        ApplyDetails(result);
    }

    winrt::fire_and_forget EventLogsViewModel::LoadAsync(
        std::uint64_t const requestVersion,
        std::wstring channel,
        ::AstralChronicle::services::QueryCancellation cancellation)
    {
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        auto const result = m_eventQuery->QueryPageWithQuery(channel, m_query, 256, true, cancellation);
        co_await wil::resume_foreground(m_dispatcher);
        if (requestVersion != m_requestVersion || cancellation != m_cancellation)
        {
            co_return;
        }

        ApplyResult(result);
    }

    void EventLogsViewModel::ApplyResult(::AstralChronicle::services::EventQueryResult const& result)
    {
        auto const itemVector = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : result.Events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
                event,
                *m_strings);
            if (event.RecordId != 0 && m_bookmarkedKeys.contains(
                    std::wstring{ event.Channel } + L"|" + std::to_wstring(event.RecordId)))
            {
                item.IsBookmarked(true);
            }
            itemVector.Append(item);
        }

        m_allEvents = itemVector;
        ApplyFilter();

        if (m_initialRecordId)
        {
            auto const recordId = *m_initialRecordId;
            m_initialRecordId.reset();
            for (auto const& item : m_events)
            {
                if (item.SortRecordId() == recordId)
                {
                    SelectedEvent(item);
                    break;
                }
            }
        }

        m_isLoading = false;
        m_statusSeverity = SeverityFor(result.Status);
        m_statusDetails.clear();

        using Status = ::AstralChronicle::services::EventQueryStatus;
        m_isAccessDenied = result.Status == Status::AccessDenied;
        switch (result.Status)
        {
        case Status::Succeeded:
            m_hasStatusMessage = false;
            m_statusText = FormatResource(
                m_strings->GetString(L"EventLogs.Loaded.Text"),
                { winrt::to_hstring(result.Events.size()) });
            break;
        case Status::NoEvents:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.NoEvents.Text");
            break;
        case Status::Cancelled:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.Cancelled.Text");
            break;
        case Status::AccessDenied:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.AccessDenied.Text");
            break;
        case Status::InvalidChannel:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.InvalidChannel.Text");
            break;
        case Status::ServiceUnavailable:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.ServiceUnavailable.Text");
            break;
        case Status::Failed:
        default:
            m_hasStatusMessage = true;
            m_statusText = m_strings->GetString(L"EventLogs.QueryFailed.Text");
            break;
        }

        if (result.ErrorCode != 0)
        {
            m_statusDetails = FormatResource(
                m_strings->GetString(L"EventLogs.ErrorDetails.Text"),
                { winrt::to_hstring(result.ErrorCode) });
        }

        RaisePropertyChanged(L"Events");
        RaiseStatusProperties();
        RaiseSelectionProperties();
    }

    void EventLogsViewModel::ApplyFilter()
    {
        if (!m_allEvents)
        {
            return;
        }

        auto const needle = Lowercase(std::wstring{ m_searchText.c_str() });
        std::vector<winrt::AstralChronicle::EventLogItemViewModel> filteredItems;
        for (auto const& item : m_allEvents)
        {
            if (needle.empty() || Contains(item, needle))
            {
                filteredItems.emplace_back(item);
            }
        }
        auto valueForSort = [this](winrt::AstralChronicle::EventLogItemViewModel const& item)
        {
            if (m_sortKey == L"Time") return std::to_wstring(item.SortTimestamp());
            if (m_sortKey == L"RecordId") return std::to_wstring(item.SortRecordId());
            if (m_sortKey == L"Level") return std::wstring{ item.Level().c_str() };
            if (m_sortKey == L"Provider") return std::wstring{ item.Provider().c_str() };
            if (m_sortKey == L"EventId") return std::wstring{ item.EventId().c_str() };
            if (m_sortKey == L"Task") return std::wstring{ item.TaskCategory().c_str() };
            if (m_sortKey == L"Channel") return std::wstring{ item.Channel().c_str() };
            if (m_sortKey == L"User") return std::wstring{ item.User().c_str() };
            if (m_sortKey == L"Computer") return std::wstring{ item.Computer().c_str() };
            if (m_sortKey == L"Description") return std::wstring{ item.ShortDescription().c_str() };
            return std::wstring{ item.RecordId().c_str() };
        };
        std::sort(filteredItems.begin(), filteredItems.end(), [&](auto const& left, auto const& right)
        {
            auto const leftValue = valueForSort(left);
            auto const rightValue = valueForSort(right);
            return m_sortAscending ? leftValue < rightValue : leftValue > rightValue;
        });
        auto filtered = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& item : filteredItems)
        {
            filtered.Append(item);
        }
        m_events = filtered;
        if (needle.empty() && !m_hasStructuredFilter)
        {
            m_filterSummary = m_strings->GetString(L"EventLogs.FilterNone.Text");
        }
        else if (m_searchText.empty() && m_hasStructuredFilter)
        {
            m_filterSummary = FormatResource(
                m_strings->GetString(L"EventLogs.FilterSummary.Structured.Text"),
                { winrt::to_hstring(m_events.Size()), winrt::to_hstring(m_allEvents.Size()) });
        }
        else
        {
            m_filterSummary = FormatResource(
                m_strings->GetString(L"EventLogs.FilterSummary.Text"),
                { winrt::to_hstring(m_events.Size()), winrt::to_hstring(m_allEvents.Size()) });
        }
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"FilterSummary");
        RaisePropertyChanged(L"HasFilter");
    }

    void EventLogsViewModel::ApplyDetails(::AstralChronicle::services::EventDetailsResult const& result)
    {
        m_isDetailsLoading = false;
        if (result.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded)
        {
            m_selectedMessage = result.Details.FormattedMessage.empty()
                ? m_strings->GetString(L"EventLogs.MessageFormattingFailed.Text")
                : winrt::hstring{ result.Details.FormattedMessage };
            m_selectedXml = result.Details.RawXml.empty()
                ? m_strings->GetString(L"EventLogs.XmlUnavailable.Text")
                : winrt::hstring{ result.Details.RawXml };
            m_selectedEventData = winrt::hstring{ FormatProperties(result.Details.EventData) };
            m_selectedUserData = winrt::hstring{ FormatProperties(result.Details.UserData) };
            m_selectedProviderMetadata = winrt::hstring{ FormatProviderMetadata(result.Details.ProviderMetadata) };
            m_selectedBinaryData = result.Details.BinaryData.empty()
                ? m_strings->GetString(L"EventLogs.EmptyValue.Text")
                : winrt::hstring{ result.Details.BinaryData };
            m_selectedRelatedEvents = result.Details.RelatedEvents.empty()
                ? m_strings->GetString(L"EventLogs.EmptyValue.Text")
                : winrt::hstring{ result.Details.RelatedEvents };
            m_detailsStatusText = result.Details.FormattingErrorCode != 0
                ? FormatResource(
                    m_strings->GetString(L"EventLogs.MessageFormattingError.Text"),
                    { winrt::to_hstring(result.Details.FormattingErrorCode) })
                : m_strings->GetString(L"EventLogs.DetailsLoaded.Text");
        }
        else
        {
            m_selectedMessage = m_strings->GetString(L"EventLogs.MessageFormattingFailed.Text");
            m_selectedXml = m_strings->GetString(L"EventLogs.XmlUnavailable.Text");
            m_selectedEventData = m_selectedXml;
            m_selectedUserData = m_selectedXml;
            m_selectedProviderMetadata = m_selectedXml;
            m_selectedBinaryData = m_selectedXml;
            m_selectedRelatedEvents = m_selectedXml;
            m_detailsStatusText = m_statusText;
        }

        RaisePropertyChanged(L"SelectedMessage");
        RaisePropertyChanged(L"SelectedXml");
        RaisePropertyChanged(L"SelectedEventData");
        RaisePropertyChanged(L"SelectedUserData");
        RaisePropertyChanged(L"SelectedProviderMetadata");
        RaisePropertyChanged(L"SelectedBinaryData");
        RaisePropertyChanged(L"SelectedRelatedEvents");
        RaisePropertyChanged(L"DetailsStatusText");
        RaisePropertyChanged(L"IsDetailsLoading");
    }

    void EventLogsViewModel::ClearSelection()
    {
        m_selectedEvent = nullptr;
        m_selectedEvents.clear();
        auto const empty = m_strings ? m_strings->GetString(L"EventLogs.EmptyValue.Text") : winrt::hstring{};
        m_selectedProvider = empty;
        m_selectedEventId = empty;
        m_selectedVersion = empty;
        m_selectedLevel = empty;
        m_selectedOpcode = empty;
        m_selectedKeywords = empty;
        m_selectedTimeCreated = empty;
        m_selectedTaskCategory = empty;
        m_selectedChannel = empty;
        m_selectedUser = empty;
        m_selectedComputer = empty;
        m_selectedRecordId = empty;
        m_selectedProcessId = empty;
        m_selectedThreadId = empty;
        m_selectedActivityId = empty;
        m_selectedRelatedActivityId = empty;
        m_selectedDescription = m_strings
            ? m_strings->GetString(L"EventLogs.SelectEvent.Text")
            : winrt::hstring{};
        m_isDetailsLoading = false;
        m_detailsStatusText = m_strings
            ? m_strings->GetString(L"EventLogs.SelectEvent.Text")
            : winrt::hstring{};
        m_selectedMessage = m_detailsStatusText;
        m_selectedXml = m_detailsStatusText;
        m_selectedEventData = m_detailsStatusText;
        m_selectedUserData = m_detailsStatusText;
        m_selectedProviderMetadata = m_detailsStatusText;
        m_selectedBinaryData = m_detailsStatusText;
        m_selectedRelatedEvents = m_detailsStatusText;
    }

    void EventLogsViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"StatusDetails");
        RaisePropertyChanged(L"HasStatusMessage");
        RaisePropertyChanged(L"IsAccessDenied");
        RaisePropertyChanged(L"IsLoading");
        RaisePropertyChanged(L"StatusSeverity");
    }

    void EventLogsViewModel::RaiseSelectionProperties()
    {
        RaisePropertyChanged(L"SelectedProvider");
        RaisePropertyChanged(L"SelectedEventId");
        RaisePropertyChanged(L"SelectedVersion");
        RaisePropertyChanged(L"SelectedLevel");
        RaisePropertyChanged(L"SelectedOpcode");
        RaisePropertyChanged(L"SelectedKeywords");
        RaisePropertyChanged(L"SelectedTimeCreated");
        RaisePropertyChanged(L"SelectedTaskCategory");
        RaisePropertyChanged(L"SelectedChannel");
        RaisePropertyChanged(L"SelectedUser");
        RaisePropertyChanged(L"SelectedComputer");
        RaisePropertyChanged(L"SelectedRecordId");
        RaisePropertyChanged(L"SelectedProcessId");
        RaisePropertyChanged(L"SelectedThreadId");
        RaisePropertyChanged(L"SelectedActivityId");
        RaisePropertyChanged(L"SelectedRelatedActivityId");
        RaisePropertyChanged(L"SelectedDescription");
        RaisePropertyChanged(L"SelectedCount");
        RaisePropertyChanged(L"SelectedMessage");
        RaisePropertyChanged(L"SelectedXml");
        RaisePropertyChanged(L"SelectedEventData");
        RaisePropertyChanged(L"SelectedUserData");
        RaisePropertyChanged(L"SelectedProviderMetadata");
        RaisePropertyChanged(L"SelectedBinaryData");
        RaisePropertyChanged(L"SelectedRelatedEvents");
        RaisePropertyChanged(L"DetailsStatusText");
        RaisePropertyChanged(L"IsDetailsLoading");
    }

    void EventLogsViewModel::RaiseFilterProperties()
    {
        RaisePropertyChanged(L"SearchText");
        RaisePropertyChanged(L"FilterProvider");
        RaisePropertyChanged(L"FilterEventId");
        RaisePropertyChanged(L"FilterLevel");
        RaisePropertyChanged(L"FilterTask");
        RaisePropertyChanged(L"FilterOpcode");
        RaisePropertyChanged(L"FilterKeyword");
        RaisePropertyChanged(L"FilterProcessId");
        RaisePropertyChanged(L"FilterThreadId");
        RaisePropertyChanged(L"FilterUser");
        RaisePropertyChanged(L"FilterComputer");
        RaisePropertyChanged(L"RawXPath");
        RaisePropertyChanged(L"FilterAfterToday");
        RaisePropertyChanged(L"FilterAfterHours");
        RaisePropertyChanged(L"HasStructuredFilter");
        RaisePropertyChanged(L"HasFilter");
    }

    void EventLogsViewModel::LoadBookmarks()
    {
        m_bookmarkedKeys.clear();
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values();
            if (!values.HasKey(L"EventLogs.BookmarkedRecordIds"))
            {
                return;
            }

            auto const stored = winrt::unbox_value<winrt::hstring>(
                values.Lookup(L"EventLogs.BookmarkedRecordIds"));
            std::wstring text{ stored.c_str(), stored.size() };
            std::size_t start{};
            while (start < text.size())
            {
                auto const separator = text.find(L';', start);
                auto const token = text.substr(
                    start,
                    separator == std::wstring::npos ? std::wstring::npos : separator - start);
                if (!token.empty())
                {
                    try
                    {
                        m_bookmarkedKeys.insert(token);
                    }
                    catch (...)
                    {
                    }
                }
                if (separator == std::wstring::npos)
                {
                    break;
                }
                start = separator + 1;
            }
        }
        catch (...)
        {
            m_bookmarkedKeys.clear();
        }
    }

    void EventLogsViewModel::PersistBookmarks() const
    {
        try
        {
            std::wstring text;
            for (auto const& bookmarkKey : m_bookmarkedKeys)
            {
                if (!text.empty())
                {
                    text += L';';
                }
                text += bookmarkKey;
            }
            winrt::Windows::Storage::ApplicationData::Current()
                .LocalSettings()
                .Values()
                .Insert(L"EventLogs.BookmarkedRecordIds", winrt::box_value(winrt::hstring{ text }));
        }
        catch (...)
        {
        }
    }

    winrt::event_token EventLogsViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void EventLogsViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void EventLogsViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
