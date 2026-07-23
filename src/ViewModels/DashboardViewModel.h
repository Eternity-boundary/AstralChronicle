// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DashboardViewModel.g.h"
#include "EventLogItemViewModel.h"
#include "Services/IEventQueryService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <string>

namespace AstralChronicle::services
{
    struct IEventQueryService;
}

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct DashboardViewModel : DashboardViewModelT<DashboardViewModel>
    {
        DashboardViewModel();

        void Initialize(
            ::AstralChronicle::services::IEventQueryService const& eventQuery,
            ::AstralChronicle::design::IStringResourceService const& strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring RecentEventPreview() const;
        [[nodiscard]] winrt::hstring ErrorCount() const;
        [[nodiscard]] winrt::hstring WarningCount() const;
        [[nodiscard]] winrt::hstring CriticalCount() const;
        [[nodiscard]] winrt::hstring TodayCount() const;
        [[nodiscard]] winrt::hstring MonitoringStatus() const;
        [[nodiscard]] winrt::hstring TimelineSummary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] bool IsLoading() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> RecentCriticalEvents() const;
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget LoadAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::wstring querySinceMidnight,
            std::wstring criticalQuery);
        void ApplyResults(
            ::AstralChronicle::services::EventLevelCountsResult const& counts,
            ::AstralChronicle::services::EventQueryResult const& criticalEvents);
        void RaiseDataProperties();
        void RaisePropertyChanged(winrt::hstring const& propertyName);

        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_recentEventPreview;
        winrt::hstring m_errorCount;
        winrt::hstring m_warningCount;
        winrt::hstring m_criticalCount;
        winrt::hstring m_todayCount;
        winrt::hstring m_monitoringStatus;
        winrt::hstring m_timelineSummary;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        ::AstralChronicle::services::IEventQueryService const* m_eventQuery{};
        ::AstralChronicle::design::IStringResourceService const* m_strings{};
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        ::AstralChronicle::services::QueryCancellation m_cancellation;
        std::uint64_t m_requestVersion{};
        bool m_hasStatusMessage{};
        bool m_isLoading{};
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_recentCriticalEvents{ nullptr };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct DashboardViewModel : DashboardViewModelT<DashboardViewModel, implementation::DashboardViewModel>
    {
    };
}
