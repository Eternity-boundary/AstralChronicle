// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "TimelineViewModel.g.h"
#include "EventLogItemViewModel.h"
#include "Services/IEventQueryService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct TimelineViewModel : TimelineViewModelT<TimelineViewModel>
    {
        TimelineViewModel();
        ~TimelineViewModel() noexcept;

        void Initialize(
            std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] bool IsLoading() const noexcept;
        [[nodiscard]] winrt::hstring SearchText() const;
        void SearchText(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring TimeRange() const;
        void TimeRange(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring LevelFilter() const;
        void LevelFilter(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring ChannelFilter() const;
        void ChannelFilter(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring ProviderFilter() const;
        void ProviderFilter(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterSummary() const;
        [[nodiscard]] winrt::hstring CorrelationSummary() const;
        [[nodiscard]] bool CorrelationEnabled() const noexcept;
        void CorrelationEnabled(bool value);
        [[nodiscard]] bool GroupRepeated() const noexcept;
        void GroupRepeated(bool value);
        [[nodiscard]] winrt::AstralChronicle::EventLogItemViewModel SelectedEvent() const;
        [[nodiscard]] winrt::hstring SelectedEventDetails() const;
        [[nodiscard]] std::uint32_t BookmarkCount() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> Events() const;
        void Refresh();
        void Select(winrt::AstralChronicle::EventLogItemViewModel const& item);
        void BookmarkSelected();
        [[nodiscard]] winrt::hstring ExportText() const;

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget LoadAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::wstring timeRange,
            std::wstring level,
            std::wstring channel,
            std::uint32_t maximumRecords);
        void ApplyResults(std::vector<::AstralChronicle::models::EventRecordSummary> events,
            ::AstralChronicle::services::EventQueryStatus status,
            std::uint32_t errorCode,
            bool partialFailure);
        void ApplySearchFilter();
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();

        std::shared_ptr<::AstralChronicle::services::IEventQueryService> m_eventQuery;
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        std::uint64_t m_requestVersion{};
        ::AstralChronicle::services::QueryCancellation m_cancellation;
        ::AstralChronicle::viewmodels::EventItemSettings m_eventItemSettings;
        std::uint32_t m_queryBatchSize{ 80 };
        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        winrt::hstring m_searchText;
        winrt::hstring m_timeRange{ L"Day" };
        winrt::hstring m_levelFilter{ L"Any" };
        winrt::hstring m_channelFilter{ L"Any" };
        winrt::hstring m_providerFilter;
        winrt::hstring m_filterSummary;
        winrt::hstring m_correlationSummary;
        winrt::AstralChronicle::EventLogItemViewModel m_selectedEvent{ nullptr };
        winrt::hstring m_selectedEventDetails;
        std::uint32_t m_bookmarkCount{};
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_events{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_allEvents{ nullptr };
        bool m_hasStatusMessage{ true };
        bool m_isLoading{};
        bool m_correlationEnabled{ true };
        bool m_groupRepeated{};
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct TimelineViewModel : TimelineViewModelT<TimelineViewModel, implementation::TimelineViewModel>
    {
    };
}
