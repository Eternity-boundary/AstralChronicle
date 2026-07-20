// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardViewModel.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/IEventLogCatalogService.h"
#include "Services/IEventQueryService.h"

#include "DashboardViewModel.g.cpp"
#include "EventLogItemViewModel.h"

#include <wil/cppwinrt_helpers.h>

#include <ctime>
#include <iomanip>
#include <sstream>
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

        [[nodiscard]] std::wstring CriticalQuery(std::wstring const& sinceQuery)
        {
            auto const marker = sinceQuery.find(L"TimeCreated[");
            if (marker == std::wstring::npos)
            {
                return L"*[System[(Level=1 or Level=2)]]";
            }

            auto const timeExpression = sinceQuery.substr(marker);
            return L"*[System[(Level=1 or Level=2) and " + timeExpression.substr(0, timeExpression.size() - 3) + L"]]]";
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
            winrt::single_threaded_vector<winrt::AstralChronicle::EventLogItemViewModel>().GetView())
    {
    }

    void DashboardViewModel::Initialize(
        ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_eventLogCatalog = &eventLogCatalog;
        m_eventQuery = &eventQuery;
        m_strings = &strings;
        m_dispatcher = dispatcher;
        m_heading = strings.GetString(L"Dashboard.Heading");
        m_summary = strings.GetString(L"Dashboard.Summary.Initial");
        m_channelCountLabel = strings.GetString(L"Dashboard.ChannelCountLoading");
        m_recentEventPreview = strings.GetString(L"Dashboard.RecentEventLoading");

        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
        m_cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_requestVersion;
        m_isLoading = true;
        m_hasStatusMessage = false;
        m_statusSeverity = winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        auto const metricLoading = strings.GetString(L"Dashboard.MetricLoading.Text");
        m_errorCount = metricLoading;
        m_warningCount = metricLoading;
        m_criticalCount = metricLoading;
        m_todayCount = metricLoading;
        m_monitoringStatus = strings.GetString(L"Dashboard.Monitoring.NotConfigured.Text");
        m_timelineSummary = strings.GetString(L"Dashboard.CrashTimelineLoading.Text");
        m_statusText = strings.GetString(L"Dashboard.Loading.Text");
        m_statusDetails = strings.GetString(L"Dashboard.LoadingDetails.Text");
        m_recentCriticalEvents = winrt::single_threaded_vector<winrt::AstralChronicle::EventLogItemViewModel>().GetView();
        RaiseDataProperties();
        auto const querySinceMidnight = QuerySinceLocalMidnight();
        LoadAsync(requestVersion, m_cancellation, querySinceMidnight, CriticalQuery(querySinceMidnight));
    }

    winrt::fire_and_forget DashboardViewModel::LoadAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring querySinceMidnight,
        std::wstring criticalQuery)
    {
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        auto const channelCount = m_eventLogCatalog->GetAvailableChannelCount();
        auto const counts = m_eventQuery->QueryLevelCounts(L"System", querySinceMidnight, cancellation);
        auto const criticalEvents = m_eventQuery->QueryPageWithQuery(
            L"System",
            criticalQuery,
            6,
            true,
            cancellation);
        co_await wil::resume_foreground(m_dispatcher);
        if (requestVersion != m_requestVersion || cancellation != m_cancellation)
        {
            co_return;
        }

        ApplyResults(channelCount, counts, criticalEvents);
    }

    void DashboardViewModel::ApplyResults(
        std::uint32_t const channelCount,
        ::AstralChronicle::services::EventLevelCountsResult const& counts,
        ::AstralChronicle::services::EventQueryResult const& criticalEvents)
    {
        m_channelCountLabel = FormatResource(
            m_strings->GetString(L"Dashboard.ChannelCountLabel"),
            { winrt::to_hstring(channelCount) });
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

        auto const eventVector = winrt::single_threaded_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : criticalEvents.Events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
                event,
                *m_strings);
            eventVector.Append(item);
        }
        m_recentCriticalEvents = eventVector.GetView();

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

    winrt::hstring DashboardViewModel::ChannelCountLabel() const
    {
        return m_channelCountLabel;
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
    Windows::Foundation::Collections::IVectorView<winrt::AstralChronicle::EventLogItemViewModel> DashboardViewModel::RecentCriticalEvents() const
    {
        return m_recentCriticalEvents;
    }

    void DashboardViewModel::RaiseDataProperties()
    {
        RaisePropertyChanged(L"Summary");
        RaisePropertyChanged(L"ChannelCountLabel");
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
