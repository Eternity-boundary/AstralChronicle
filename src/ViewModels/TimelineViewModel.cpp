// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "TimelineViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "EventLogItemViewModel.h"

#include "TimelineViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

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

    [[nodiscard]] std::wstring QueryFor(
        std::wstring const& timeRange,
        std::wstring const& level)
    {
        std::wstring timeExpression;
        if (timeRange == L"Hour") timeExpression = L"TimeCreated[timediff(@SystemTime) <= 3600000]";
        else if (timeRange == L"Day") timeExpression = L"TimeCreated[timediff(@SystemTime) <= 86400000]";
        else if (timeRange == L"Week") timeExpression = L"TimeCreated[timediff(@SystemTime) <= 604800000]";
        else if (timeRange == L"Month") timeExpression = L"TimeCreated[timediff(@SystemTime) <= 2592000000]";

        std::vector<std::wstring> predicates;
        if (!timeExpression.empty()) predicates.emplace_back(timeExpression);
        if (level == L"Critical") predicates.emplace_back(L"Level=1");
        else if (level == L"Error") predicates.emplace_back(L"Level=2");
        else if (level == L"Warning") predicates.emplace_back(L"Level=3");
        else if (level == L"Information") predicates.emplace_back(L"Level=4");
        if (predicates.empty()) return L"*";

        std::wstring query{ L"*[System[" };
        for (std::size_t index{}; index < predicates.size(); ++index)
        {
            if (index != 0) query += L" and ";
            query += predicates[index];
        }
        query += L"]]";
        return query;
    }

    [[nodiscard]] bool Contains(
        winrt::AstralChronicle::EventLogItemViewModel const& item,
        std::wstring const& needle)
    {
        auto const matches = [&needle](winrt::hstring const& value)
        {
            std::wstring text{ value.c_str() };
            std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return std::towlower(c); });
            return text.find(needle) != std::wstring::npos;
        };
        return matches(item.TimeCreated()) || matches(item.Level()) || matches(item.Provider()) ||
            matches(item.EventId()) || matches(item.Channel()) || matches(item.ShortDescription());
    }

    [[nodiscard]] winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity SeverityFor(
        AstralChronicle::services::EventQueryStatus const status)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        return status == Status::AccessDenied || status == Status::InvalidChannel ||
            status == Status::ServiceUnavailable || status == Status::Failed
            ? winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error
            : winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
    }

    [[nodiscard]] std::wstring EscapeTsvField(winrt::hstring const& value)
    {
        auto result = std::wstring{ value.c_str(), value.size() };
        for (auto& character : result)
        {
            if (character == L'\t' || character == L'\r' || character == L'\n')
            {
                character = L' ';
            }
        }
        return result;
    }
}

namespace winrt::AstralChronicle::implementation
{
    TimelineViewModel::TimelineViewModel()
        : m_events(winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>())
    {
    }

    TimelineViewModel::~TimelineViewModel() noexcept
    {
        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
    }

    void TimelineViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_eventQuery = std::move(eventQuery);
        m_strings = std::move(strings);
        if (!m_eventQuery || !m_strings)
        {
            throw std::invalid_argument("Timeline requires query and string services.");
        }
        m_dispatcher = dispatcher;
        auto const settings = ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load();
        m_eventItemSettings = settings.EventItems;
        m_queryBatchSize = settings.QueryBatchSize;
        m_groupRepeated = settings.GroupRepeatedEvents;
        m_heading = m_strings->GetString(L"Timeline.Heading");
        m_summary = m_strings->GetString(L"Timeline.Summary");
        m_filterSummary = m_strings->GetString(L"Timeline.FilterNone.Text");
        m_correlationSummary = m_strings->GetString(L"Timeline.Correlation.Text");
        Refresh();
    }

    void TimelineViewModel::Refresh()
    {
        if (!m_eventQuery || !m_strings || !m_dispatcher) return;
        if (m_cancellation) m_cancellation->store(true, std::memory_order_relaxed);
        m_cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_requestVersion;
        m_isLoading = true;
        m_isAccessDenied = false;
        m_hasStatusMessage = true;
        m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        m_statusText = m_strings->GetString(L"Timeline.Loading.Text");
        m_statusDetails = m_strings->GetString(L"Timeline.LoadingDetails.Text");
        RaiseStatusProperties();
        LoadAsync(requestVersion, m_cancellation,
            std::wstring{ m_timeRange.c_str() },
            std::wstring{ m_levelFilter.c_str() },
            std::wstring{ m_channelFilter.c_str() },
            m_queryBatchSize);
    }

    winrt::fire_and_forget TimelineViewModel::LoadAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring timeRange,
        std::wstring level,
        std::wstring channel,
        std::uint32_t const maximumRecords)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const eventQuery = m_eventQuery;
            auto const dispatcher = m_dispatcher;
            if (!eventQuery || !dispatcher) co_return;
            co_await winrt::resume_background();
            std::vector<::AstralChronicle::models::EventRecordSummary> events;
            auto status = ::AstralChronicle::services::EventQueryStatus::NoEvents;
            auto failureStatus = ::AstralChronicle::services::EventQueryStatus::NoEvents;
            std::uint32_t errorCode{};
            bool hadFailure{};
            auto const query = QueryFor(timeRange, level);
            std::vector<std::wstring> channels = channel == L"Any"
                ? std::vector<std::wstring>{ L"System", L"Application" }
                : std::vector<std::wstring>{ channel };
            for (auto const& currentChannel : channels)
            {
                auto const result = eventQuery->QueryPageWithQuery(currentChannel, query, maximumRecords, true, cancellation);
                if (result.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded)
                {
                    events.insert(events.end(), result.Events.begin(), result.Events.end());
                    status = result.Status;
                }
                else if (result.Status != ::AstralChronicle::services::EventQueryStatus::NoEvents &&
                    result.Status != ::AstralChronicle::services::EventQueryStatus::Cancelled)
                {
                    if (!hadFailure)
                    {
                        failureStatus = result.Status;
                        errorCode = result.ErrorCode;
                    }
                    hadFailure = true;
                }
                if (cancellation->load(std::memory_order_relaxed) ||
                    result.Status == ::AstralChronicle::services::EventQueryStatus::Cancelled)
                {
                    status = ::AstralChronicle::services::EventQueryStatus::Cancelled;
                    errorCode = result.ErrorCode;
                    break;
                }
            }
            auto const cancelled = status == ::AstralChronicle::services::EventQueryStatus::Cancelled;
            auto const partialFailure = !cancelled && hadFailure && !events.empty();
            if (!cancelled && events.empty() && hadFailure)
            {
                status = failureStatus;
            }
            std::sort(events.begin(), events.end(), [](auto const& left, auto const& right)
            {
                return left.TimeCreated > right.TimeCreated;
            });
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || requestVersion != strongThis->m_requestVersion || cancellation != strongThis->m_cancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            strongThis->ApplyResults(std::move(events), status, errorCode, partialFailure);
        }
        catch (...)
        {
            co_return;
        }
    }

    void TimelineViewModel::ApplyResults(
        std::vector<::AstralChronicle::models::EventRecordSummary> events,
        ::AstralChronicle::services::EventQueryStatus const status,
        std::uint32_t const errorCode,
        bool const partialFailure)
    {
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
                event,
                *m_strings,
                m_eventItemSettings);
            values.Append(item);
        }
        m_allEvents = values;
        m_selectedEvent = nullptr;
        m_selectedEventDetails.clear();
        m_bookmarkCount = 0;
        ApplySearchFilter();
        m_isLoading = false;
        m_statusSeverity = SeverityFor(status);
        m_isAccessDenied = status == ::AstralChronicle::services::EventQueryStatus::AccessDenied;
        m_statusDetails.clear();
        if (!partialFailure && (status == ::AstralChronicle::services::EventQueryStatus::Succeeded ||
            status == ::AstralChronicle::services::EventQueryStatus::NoEvents)
        )
        {
            m_hasStatusMessage = false;
            m_statusText = m_strings->GetString(L"Timeline.Ready.Text");
            m_summary = FormatResource(m_strings->GetString(L"Timeline.SummaryCount.Text"), { winrt::to_hstring(events.size()) });
        }
        else
        {
            m_hasStatusMessage = true;
            if (partialFailure)
            {
                m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning;
            }
            m_statusText = m_strings->GetString(m_isAccessDenied
                ? L"Timeline.AccessDenied.Text"
                : L"Timeline.QueryFailed.Text");
            m_statusDetails = FormatResource(m_strings->GetString(L"Timeline.ErrorDetails.Text"), { winrt::to_hstring(errorCode) });
            m_summary = FormatResource(m_strings->GetString(L"Timeline.SummaryCount.Text"), { winrt::to_hstring(events.size()) });
        }
        RaiseStatusProperties();
        RaisePropertyChanged(L"Summary");
    }

    void TimelineViewModel::ApplySearchFilter()
    {
        std::wstring needle{ m_searchText.c_str() };
        std::transform(needle.begin(), needle.end(), needle.begin(), [](wchar_t c) { return std::towlower(c); });
        std::wstring providerNeedle{ m_providerFilter.c_str() };
        std::transform(providerNeedle.begin(), providerNeedle.end(), providerNeedle.begin(), [](wchar_t c) { return std::towlower(c); });
        std::unordered_set<std::wstring> grouped;
        auto filtered = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        if (m_allEvents)
        {
            for (auto const& item : m_allEvents)
            {
                std::wstring provider{ item.Provider().c_str() };
                std::transform(provider.begin(), provider.end(), provider.begin(), [](wchar_t c) { return std::towlower(c); });
                if (!providerNeedle.empty() && provider.find(providerNeedle) == std::wstring::npos) continue;
                if (!needle.empty() && !Contains(item, needle)) continue;
                if (m_groupRepeated)
                {
                    auto key = std::wstring{ item.Provider().c_str() } + L"|" + item.EventId().c_str() + L"|" + item.Channel().c_str();
                    if (!grouped.insert(std::move(key)).second) continue;
                }
                filtered.Append(item);
            }
        }
        m_events = filtered;
        if (m_selectedEvent)
        {
            bool selectionVisible{};
            for (auto const& item : m_events)
            {
                if (item == m_selectedEvent)
                {
                    selectionVisible = true;
                    break;
                }
            }
            if (!selectionVisible)
            {
                m_selectedEvent = nullptr;
                m_selectedEventDetails.clear();
                RaisePropertyChanged(L"SelectedEvent");
                RaisePropertyChanged(L"SelectedEventDetails");
            }
        }
        m_filterSummary = FormatResource(
            m_strings->GetString(L"Timeline.FilterSummary.Text"),
            { winrt::to_hstring(m_events.Size()), winrt::to_hstring(m_allEvents ? m_allEvents.Size() : 0) });
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"FilterSummary");
        RaisePropertyChanged(L"BookmarkCount");
    }

    winrt::hstring TimelineViewModel::Heading() const { return m_heading; }
    winrt::hstring TimelineViewModel::Summary() const { return m_summary; }
    winrt::hstring TimelineViewModel::StatusText() const { return m_statusText; }
    winrt::hstring TimelineViewModel::StatusDetails() const { return m_statusDetails; }
    Microsoft::UI::Xaml::Controls::InfoBarSeverity TimelineViewModel::StatusSeverity() const noexcept { return m_statusSeverity; }
    bool TimelineViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    bool TimelineViewModel::IsAccessDenied() const noexcept { return m_isAccessDenied; }
    bool TimelineViewModel::IsLoading() const noexcept { return m_isLoading; }
    winrt::hstring TimelineViewModel::SearchText() const { return m_searchText; }
    void TimelineViewModel::SearchText(winrt::hstring const& value) { m_searchText = value; ApplySearchFilter(); RaisePropertyChanged(L"SearchText"); }
    winrt::hstring TimelineViewModel::TimeRange() const { return m_timeRange; }
    void TimelineViewModel::TimeRange(winrt::hstring const& value) { if (m_timeRange != value) { m_timeRange = value; RaisePropertyChanged(L"TimeRange"); Refresh(); } }
    winrt::hstring TimelineViewModel::LevelFilter() const { return m_levelFilter; }
    void TimelineViewModel::LevelFilter(winrt::hstring const& value) { if (m_levelFilter != value) { m_levelFilter = value; RaisePropertyChanged(L"LevelFilter"); Refresh(); } }
    winrt::hstring TimelineViewModel::ChannelFilter() const { return m_channelFilter; }
    void TimelineViewModel::ChannelFilter(winrt::hstring const& value) { if (m_channelFilter != value) { m_channelFilter = value; RaisePropertyChanged(L"ChannelFilter"); Refresh(); } }
    winrt::hstring TimelineViewModel::ProviderFilter() const { return m_providerFilter; }
    void TimelineViewModel::ProviderFilter(winrt::hstring const& value) { if (m_providerFilter != value) { m_providerFilter = value; ApplySearchFilter(); RaisePropertyChanged(L"ProviderFilter"); } }
    winrt::hstring TimelineViewModel::FilterSummary() const { return m_filterSummary; }
    winrt::hstring TimelineViewModel::CorrelationSummary() const { return m_correlationSummary; }
    bool TimelineViewModel::CorrelationEnabled() const noexcept { return m_correlationEnabled; }
    void TimelineViewModel::CorrelationEnabled(bool const value) { if (m_correlationEnabled != value) { m_correlationEnabled = value; RaisePropertyChanged(L"CorrelationEnabled"); } }
    bool TimelineViewModel::GroupRepeated() const noexcept { return m_groupRepeated; }
    void TimelineViewModel::GroupRepeated(bool const value) { if (m_groupRepeated != value) { m_groupRepeated = value; ApplySearchFilter(); RaisePropertyChanged(L"GroupRepeated"); } }
    winrt::AstralChronicle::EventLogItemViewModel TimelineViewModel::SelectedEvent() const { return m_selectedEvent; }
    winrt::hstring TimelineViewModel::SelectedEventDetails() const { return m_selectedEventDetails; }
    std::uint32_t TimelineViewModel::BookmarkCount() const noexcept { return m_bookmarkCount; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> TimelineViewModel::Events() const { return m_events; }

    void TimelineViewModel::Select(winrt::AstralChronicle::EventLogItemViewModel const& item)
    {
        m_selectedEvent = item;
        if (item)
        {
            m_selectedEventDetails = std::wstring{ L"Time: " } + item.TimeCreated().c_str() + L"\nLevel: " + item.Level().c_str() +
                L"\nProvider: " + item.Provider().c_str() + L"\nEvent ID: " + item.EventId().c_str() +
                L"\nChannel: " + item.Channel().c_str() + L"\nRecord ID: " + item.RecordId().c_str() +
                L"\nProcess: " + item.ProcessId().c_str() + L"  Thread: " + item.ThreadId().c_str() +
                L"\n\n" + item.ShortDescription().c_str();
        }
        else m_selectedEventDetails.clear();
        RaisePropertyChanged(L"SelectedEvent");
        RaisePropertyChanged(L"SelectedEventDetails");
    }

    void TimelineViewModel::BookmarkSelected()
    {
        if (!m_selectedEvent) return;
        m_selectedEvent.IsBookmarked(!m_selectedEvent.IsBookmarked());
        m_bookmarkCount = 0;
        if (m_allEvents)
        {
            for (auto const& item : m_allEvents) if (item.IsBookmarked()) ++m_bookmarkCount;
        }
        RaisePropertyChanged(L"BookmarkCount");
    }

    winrt::hstring TimelineViewModel::ExportText() const
    {
        std::wstring result{ L"Time\tLevel\tProvider\tEventId\tChannel\tDescription\r\n" };
        if (m_events)
        {
            for (auto const& item : m_events)
            {
                result += EscapeTsvField(item.TimeCreated()) + L"\t" + EscapeTsvField(item.Level()) + L"\t" +
                    EscapeTsvField(item.Provider()) + L"\t" + EscapeTsvField(item.EventId()) + L"\t" +
                    EscapeTsvField(item.Channel()) + L"\t" + EscapeTsvField(item.ShortDescription()) + L"\r\n";
            }
        }
        return winrt::hstring{ result };
    }

    void TimelineViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"StatusDetails");
        RaisePropertyChanged(L"StatusSeverity");
        RaisePropertyChanged(L"HasStatusMessage");
        RaisePropertyChanged(L"IsAccessDenied");
        RaisePropertyChanged(L"IsLoading");
    }
    winrt::event_token TimelineViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_propertyChanged.add(handler); }
    void TimelineViewModel::PropertyChanged(winrt::event_token const& token) noexcept { m_propertyChanged.remove(token); }
    void TimelineViewModel::RaisePropertyChanged(winrt::hstring const& propertyName) { m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName }); }
}
