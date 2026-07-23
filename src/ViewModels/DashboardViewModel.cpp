// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardViewModel.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/IEventQueryService.h"

#include "DashboardViewModel.g.cpp"
#include "EventLogItemViewModel.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace winrt::AstralChronicle::implementation
{
    namespace
    {
        winrt::hstring FormatResource(
            winrt::hstring format,
            std::vector<winrt::hstring> const& values)
        {
            auto result = std::wstring{ format.c_str() };
            for (std::size_t index = 0; index < values.size(); ++index)
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

        [[nodiscard]] std::wstring QuerySinceLocalMidnight()
        {
            auto const now = std::time(nullptr);
            std::tm localTime{};
            if (localtime_s(&localTime, &now) != 0)
            {
                return L"*[System[TimeCreated[timediff(@SystemTime) <= 86400000]]]";
            }

            localTime.tm_hour = 0;
            localTime.tm_min = 0;
            localTime.tm_sec = 0;
            localTime.tm_isdst = -1;
            auto const localMidnight = std::mktime(&localTime);
            if (localMidnight == static_cast<std::time_t>(-1))
            {
                return L"*[System[TimeCreated[timediff(@SystemTime) <= 86400000]]]";
            }

            std::tm utcTime{};
            if (gmtime_s(&utcTime, &localMidnight) != 0)
            {
                return L"*[System[TimeCreated[timediff(@SystemTime) <= 86400000]]]";
            }

            std::wostringstream timestamp;
            timestamp << std::setfill(L'0')
                      << std::setw(4) << utcTime.tm_year + 1900
                      << L'-' << std::setw(2) << utcTime.tm_mon + 1
                      << L'-' << std::setw(2) << utcTime.tm_mday
                      << L'T' << std::setw(2) << utcTime.tm_hour
                      << L':' << std::setw(2) << utcTime.tm_min
                      << L':' << std::setw(2) << utcTime.tm_sec
                      << L".000Z";

            return L"*[System[TimeCreated[@SystemTime >= '" + timestamp.str() + L"']]]";
        }

        [[nodiscard]] std::wstring LevelFilteredQuery(
            std::wstring const& sinceQuery,
            std::wstring_view const levelExpression)
        {
            auto const marker = sinceQuery.find(L"TimeCreated[");
            if (marker == std::wstring::npos)
            {
                return L"*[System[(" + std::wstring{ levelExpression } + L")]]";
            }

            auto const timeExpression = sinceQuery.substr(marker);
            if (timeExpression.size() < 3)
            {
                return L"*[System[(" + std::wstring{ levelExpression } + L")]]";
            }
            return L"*[System[(" + std::wstring{ levelExpression } + L") and " +
                timeExpression.substr(0, timeExpression.size() - 3) + L"]]]";
        }

        [[nodiscard]] winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity SeverityFor(
            ::AstralChronicle::services::EventQueryStatus const status)
        {
            using Status = ::AstralChronicle::services::EventQueryStatus;
            switch (status)
            {
            case Status::AccessDenied:
            case Status::InvalidChannel:
            case Status::ServiceUnavailable:
            case Status::Failed:
                return winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error;
            default:
                return winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
            }
        }
    }

    DashboardViewModel::DashboardViewModel()
        : m_recentCriticalEvents(
            winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>())
    {
    }

    void DashboardViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_eventQuery = std::move(eventQuery);
        m_strings = std::move(strings);
        if (!m_eventQuery || !m_strings)
        {
            throw std::invalid_argument("Dashboard requires query and string services.");
        }
        m_dispatcher = dispatcher;
        m_eventItemSettings = ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load().EventItems;
        m_heading = m_strings->GetString(L"Dashboard.Heading");
        m_summary = m_strings->GetString(L"Dashboard.Summary.Initial");
        m_recentEventPreview = m_strings->GetString(L"Dashboard.RecentEventLoading");

        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
        m_cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_requestVersion;
        m_isLoading = true;
        m_hasStatusMessage = false;
        m_statusSeverity = winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        auto const metricLoading = m_strings->GetString(L"Dashboard.MetricLoading.Text");
        m_errorCount = metricLoading;
        m_warningCount = metricLoading;
        m_criticalCount = metricLoading;
        m_todayCount = metricLoading;
        m_monitoringStatus = m_strings->GetString(L"Dashboard.Monitoring.NotConfigured.Text");
        m_timelineSummary = m_strings->GetString(L"Dashboard.CrashTimelineLoading.Text");
        m_statusText = m_strings->GetString(L"Dashboard.Loading.Text");
        m_statusDetails = m_strings->GetString(L"Dashboard.LoadingDetails.Text");
        m_recentCriticalEvents = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        RaiseDataProperties();
        auto const querySinceMidnight = QuerySinceLocalMidnight();
        LoadAsync(
            requestVersion,
            m_cancellation,
            querySinceMidnight,
            LevelFilteredQuery(querySinceMidnight, L"Level=1 or Level=2"));
    }

    std::wstring DashboardViewModel::QueryForTodayLevel(std::uint8_t const level)
    {
        if (level < 1 || level > 5)
        {
            return L"*[System[Level=0]]";
        }
        return LevelFilteredQuery(
            QuerySinceLocalMidnight(),
            L"Level=" + std::to_wstring(level));
    }

    std::wstring DashboardViewModel::QueryForTodayCriticalEvents()
    {
        return LevelFilteredQuery(
            QuerySinceLocalMidnight(),
            L"Level=1 or Level=2");
    }

    winrt::fire_and_forget DashboardViewModel::LoadAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring querySinceMidnight,
        std::wstring criticalQuery)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const eventQuery = m_eventQuery;
            auto const dispatcher = m_dispatcher;
            if (!eventQuery || !dispatcher) co_return;
            co_await winrt::resume_background();

            ::AstralChronicle::services::EventLevelCountsResult counts;
            ::AstralChronicle::services::EventQueryResult criticalEvents;
            try
            {
                counts = eventQuery->QueryLevelCounts(L"System", querySinceMidnight, cancellation);
                criticalEvents = eventQuery->QueryPageWithQuery(
                    L"System",
                    criticalQuery,
                    6,
                    true,
                    cancellation);
            }
            catch (winrt::hresult_error const& exception)
            {
                auto const error = static_cast<std::uint32_t>(exception.code().value);
                counts.ErrorCode = error;
                criticalEvents.ErrorCode = error;
            }
            catch (...)
            {
                counts.ErrorCode = static_cast<std::uint32_t>(E_FAIL);
                criticalEvents.ErrorCode = static_cast<std::uint32_t>(E_FAIL);
            }

            auto const queued = dispatcher.TryEnqueue(
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal,
                [weakThis, requestVersion, cancellation,
                 counts = std::move(counts), criticalEvents = std::move(criticalEvents)]()
                {
                    auto const strongThis = weakThis.get();
                    if (!strongThis || requestVersion != strongThis->m_requestVersion || cancellation != strongThis->m_cancellation ||
                        cancellation->load(std::memory_order_relaxed))
                    {
                        return;
                    }

                    strongThis->ApplyResults(counts, criticalEvents);
                });

            if (!queued)
            {
                co_return;
            }
        }
        catch (...)
        {
            co_return;
        }
    }

    void DashboardViewModel::ApplyResults(
        ::AstralChronicle::services::EventLevelCountsResult const& counts,
        ::AstralChronicle::services::EventQueryResult const& criticalEvents)
    {
        m_monitoringStatus = m_strings->GetString(L"Dashboard.Monitoring.NotConfigured.Text");
        m_isLoading = false;
        m_statusDetails.clear();

        auto const countsAvailable = counts.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded ||
            counts.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents;
        if (countsAvailable)
        {
            m_errorCount = winrt::to_hstring(counts.Counts.Error);
            m_warningCount = winrt::to_hstring(counts.Counts.Warning);
            m_criticalCount = winrt::to_hstring(counts.Counts.Critical);
            m_todayCount = winrt::to_hstring(counts.Counts.Total);
            m_summary = m_strings->GetString(L"Dashboard.Summary.Ready");
        }
        else
        {
            auto const unavailable = m_strings->GetString(L"Dashboard.MetricUnavailable.Text");
            m_errorCount = unavailable;
            m_warningCount = unavailable;
            m_criticalCount = unavailable;
            m_todayCount = unavailable;
            m_summary = m_strings->GetString(L"Dashboard.QueryUnavailable.Text");
            m_hasStatusMessage = true;
            m_statusSeverity = SeverityFor(counts.Status);
            m_statusText = m_strings->GetString(L"Dashboard.QueryFailed.Text");
            m_statusDetails = FormatResource(
                m_strings->GetString(L"Dashboard.ErrorDetails.Text"),
                { winrt::to_hstring(counts.ErrorCode) });
        }

        auto const eventVector = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : criticalEvents.Events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
                event,
                *m_strings,
                m_eventItemSettings);
            eventVector.Append(item);
        }
        m_recentCriticalEvents = eventVector;

        if (!criticalEvents.Events.empty())
        {
            auto const& latest = criticalEvents.Events.front();
            m_recentEventPreview = FormatResource(
                m_strings->GetString(L"Dashboard.RecentEventTemplate"),
                {
                    winrt::hstring{ latest.Provider },
                    winrt::to_hstring(latest.EventId),
                    winrt::to_hstring(latest.RecordId)
                });
            m_timelineSummary = FormatResource(
                m_strings->GetString(L"Dashboard.CrashTimeline.Suggested.Text"),
                { winrt::hstring{ latest.Provider }, winrt::to_hstring(latest.EventId) });
        }
        else
        {
            m_recentEventPreview = m_strings->GetString(L"Dashboard.NoSystemEvents");
            m_timelineSummary = m_strings->GetString(L"Dashboard.CrashTimeline.NoData.Text");
        }

        if (criticalEvents.Status != ::AstralChronicle::services::EventQueryStatus::Succeeded &&
            criticalEvents.Status != ::AstralChronicle::services::EventQueryStatus::NoEvents &&
            !m_hasStatusMessage)
        {
            m_hasStatusMessage = true;
            m_statusSeverity = SeverityFor(criticalEvents.Status);
            m_statusText = m_strings->GetString(L"Dashboard.QueryFailed.Text");
            m_statusDetails = FormatResource(
                m_strings->GetString(L"Dashboard.ErrorDetails.Text"),
                { winrt::to_hstring(criticalEvents.ErrorCode) });
        }

        RaiseDataProperties();
    }

    winrt::hstring DashboardViewModel::Heading() const
    {
        return m_heading;
    }

    winrt::hstring DashboardViewModel::Summary() const
    {
        return m_summary;
    }

    winrt::hstring DashboardViewModel::RecentEventPreview() const
    {
        return m_recentEventPreview;
    }

    winrt::hstring DashboardViewModel::ErrorCount() const { return m_errorCount; }
    winrt::hstring DashboardViewModel::WarningCount() const { return m_warningCount; }
    winrt::hstring DashboardViewModel::CriticalCount() const { return m_criticalCount; }
    winrt::hstring DashboardViewModel::TodayCount() const { return m_todayCount; }
    winrt::hstring DashboardViewModel::MonitoringStatus() const { return m_monitoringStatus; }
    winrt::hstring DashboardViewModel::TimelineSummary() const { return m_timelineSummary; }
    winrt::hstring DashboardViewModel::StatusText() const { return m_statusText; }
    winrt::hstring DashboardViewModel::StatusDetails() const { return m_statusDetails; }
    winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity DashboardViewModel::StatusSeverity() const noexcept
    {
        return m_statusSeverity;
    }
    bool DashboardViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    bool DashboardViewModel::IsLoading() const noexcept { return m_isLoading; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> DashboardViewModel::RecentCriticalEvents() const
    {
        return m_recentCriticalEvents;
    }

    void DashboardViewModel::RaiseDataProperties()
    {
        RaisePropertyChanged(L"Summary");
        RaisePropertyChanged(L"RecentEventPreview");
        RaisePropertyChanged(L"ErrorCount");
        RaisePropertyChanged(L"WarningCount");
        RaisePropertyChanged(L"CriticalCount");
        RaisePropertyChanged(L"TodayCount");
        RaisePropertyChanged(L"MonitoringStatus");
        RaisePropertyChanged(L"TimelineSummary");
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"StatusDetails");
        RaisePropertyChanged(L"StatusSeverity");
        RaisePropertyChanged(L"HasStatusMessage");
        RaisePropertyChanged(L"IsLoading");
        RaisePropertyChanged(L"RecentCriticalEvents");
    }

    winrt::event_token DashboardViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void DashboardViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void DashboardViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
