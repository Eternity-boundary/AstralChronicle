// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "LiveViewModel.g.h"
#include "EventLogItemViewModel.h"
#include "Services/IEventLiveService.h"
#include "Services/IEventLiveDataService.h"
#include "Services/IEventQueryService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstddef>
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
    struct LiveUpdateDispatchState;

    struct LiveEventEntry final
    {
        winrt::AstralChronicle::EventLogItemViewModel Item{ nullptr };
        ::AstralChronicle::models::LiveEventRecord Record;
    };

    struct LiveViewModel : LiveViewModelT<LiveViewModel>
    {
        LiveViewModel();
        ~LiveViewModel() noexcept;

        void Initialize(
            std::shared_ptr<::AstralChronicle::services::IEventLiveService> liveService,
            std::shared_ptr<::AstralChronicle::services::IEventLiveDataService> liveEventData,
            std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] winrt::hstring StateText() const;
        [[nodiscard]] winrt::hstring Channel() const;
        void Channel(winrt::hstring const& value);
        [[nodiscard]] std::int32_t ChannelIndex() const noexcept;
        void ChannelIndex(std::int32_t value);
        [[nodiscard]] winrt::hstring Query() const;
        void Query(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring QueueLimit() const;
        void QueueLimit(winrt::hstring const& value);
        [[nodiscard]] bool AutoScroll() const noexcept;
        void AutoScroll(bool value);
        [[nodiscard]] bool GroupRepeated() const noexcept;
        void GroupRepeated(bool value);
        [[nodiscard]] bool ShowCritical() const noexcept;
        void ShowCritical(bool value);
        [[nodiscard]] bool ShowErrors() const noexcept;
        void ShowErrors(bool value);
        [[nodiscard]] bool ShowWarnings() const noexcept;
        void ShowWarnings(bool value);
        [[nodiscard]] bool ShowInformation() const noexcept;
        void ShowInformation(bool value);
        [[nodiscard]] bool IsRecording() const noexcept;
        [[nodiscard]] winrt::hstring EventsPerSecond() const;
        [[nodiscard]] std::uint64_t TotalReceived() const noexcept;
        [[nodiscard]] std::uint32_t CriticalCount() const noexcept;
        [[nodiscard]] std::uint32_t ErrorCount() const noexcept;
        [[nodiscard]] std::uint32_t WarningCount() const noexcept;
        [[nodiscard]] std::uint32_t QueueDepth() const noexcept;
        [[nodiscard]] winrt::hstring Duration() const;
        [[nodiscard]] std::uint32_t BookmarkCount() const noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] bool IsPaused() const noexcept;
        [[nodiscard]] bool CanStart() const noexcept;
        [[nodiscard]] std::uint32_t DroppedCount() const noexcept;
        [[nodiscard]] std::uint32_t EventCount() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> Events() const;
        [[nodiscard]] winrt::AstralChronicle::EventLogItemViewModel SelectedEvent() const;
        void SelectedEvent(winrt::AstralChronicle::EventLogItemViewModel const& value);
        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] winrt::hstring SelectedProvider() const;
        [[nodiscard]] winrt::hstring SelectedEventId() const;
        [[nodiscard]] winrt::hstring SelectedVersion() const;
        [[nodiscard]] winrt::hstring SelectedLevel() const;
        [[nodiscard]] winrt::hstring SelectedOpcode() const;
        [[nodiscard]] winrt::hstring SelectedKeywords() const;
        [[nodiscard]] winrt::hstring SelectedTimeCreated() const;
        [[nodiscard]] winrt::hstring SelectedTaskCategory() const;
        [[nodiscard]] winrt::hstring SelectedChannel() const;
        [[nodiscard]] winrt::hstring SelectedUser() const;
        [[nodiscard]] winrt::hstring SelectedComputer() const;
        [[nodiscard]] winrt::hstring SelectedRecordId() const;
        [[nodiscard]] winrt::hstring SelectedProcessId() const;
        [[nodiscard]] winrt::hstring SelectedThreadId() const;
        [[nodiscard]] winrt::hstring SelectedActivityId() const;
        [[nodiscard]] winrt::hstring SelectedRelatedActivityId() const;
        [[nodiscard]] winrt::hstring SelectedDescription() const;
        [[nodiscard]] winrt::hstring SelectedMessage() const;
        [[nodiscard]] winrt::hstring SelectedXml() const;
        [[nodiscard]] winrt::hstring SelectedEventData() const;
        [[nodiscard]] winrt::hstring SelectedUserData() const;
        [[nodiscard]] winrt::hstring SelectedProviderMetadata() const;
        [[nodiscard]] winrt::hstring SelectedBinaryData() const;
        [[nodiscard]] winrt::hstring SelectedRelatedEvents() const;
        [[nodiscard]] winrt::hstring DetailsStatusText() const;
        [[nodiscard]] bool IsDetailsLoading() const noexcept;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        void Start();
        void Pause();
        void Resume();
        void Stop();
        void Clear();
        void ToggleRecording();
        void Export();
        void BookmarkLatest();
        void Shutdown() noexcept;

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void DrainAvailableEvents();
        void ApplyBatch(::AstralChronicle::services::EventLiveBatch batch);
        void RebuildEventView();
        [[nodiscard]] bool AppendEvent(::AstralChronicle::models::LiveEventRecord record);
        [[nodiscard]] bool MatchesLevelFilter(std::uint8_t level) const noexcept;
        [[nodiscard]] LiveEventEntry const* FindEvent(winrt::AstralChronicle::EventLogItemViewModel const& item) const noexcept;
        winrt::fire_and_forget LoadDetailsAsync(
            std::uint64_t requestVersion,
            std::wstring channel,
            std::uint64_t recordId,
            ::AstralChronicle::models::LiveEventRecord record,
            ::AstralChronicle::services::QueryCancellation cancellation);
        void ApplyDetails(
            ::AstralChronicle::models::EventDetails const& details,
            bool querySucceeded);
        void ClearSelection();
        void SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();
        void RaiseMetricProperties();
        void RaiseSelectionProperties();
        winrt::fire_and_forget ExportAsync(
            std::vector<std::wstring> events,
            std::uint64_t lifetimeVersion,
            bool redactComputerName,
            bool redactUserNames);

        std::shared_ptr<::AstralChronicle::services::IEventLiveService> m_liveService;
        std::shared_ptr<::AstralChronicle::services::IEventLiveDataService> m_liveEventData;
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> m_eventQuery;
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        std::shared_ptr<LiveUpdateDispatchState> m_liveUpdateDispatch;
        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        winrt::hstring m_stateText;
        winrt::hstring m_channel{ L"System" };
        winrt::hstring m_query{ L"*" };
        winrt::hstring m_queueLimit{ L"5000" };
        winrt::hstring m_eventsPerSecond{ L"0" };
        winrt::hstring m_duration{ L"0s" };
        winrt::hstring m_selectedProvider;
        winrt::hstring m_selectedEventId;
        winrt::hstring m_selectedVersion;
        winrt::hstring m_selectedLevel;
        winrt::hstring m_selectedOpcode;
        winrt::hstring m_selectedKeywords;
        winrt::hstring m_selectedTimeCreated;
        winrt::hstring m_selectedTaskCategory;
        winrt::hstring m_selectedChannel;
        winrt::hstring m_selectedUser;
        winrt::hstring m_selectedComputer;
        winrt::hstring m_selectedRecordId;
        winrt::hstring m_selectedProcessId;
        winrt::hstring m_selectedThreadId;
        winrt::hstring m_selectedActivityId;
        winrt::hstring m_selectedRelatedActivityId;
        winrt::hstring m_selectedDescription;
        winrt::hstring m_selectedMessage;
        winrt::hstring m_selectedXml;
        winrt::hstring m_selectedEventData;
        winrt::hstring m_selectedUserData;
        winrt::hstring m_selectedProviderMetadata;
        winrt::hstring m_selectedBinaryData;
        winrt::hstring m_selectedRelatedEvents;
        winrt::hstring m_detailsStatusText;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_events{ nullptr };
        std::vector<LiveEventEntry> m_allEvents;
        std::vector<std::wstring> m_recordedEvents;
        winrt::AstralChronicle::EventLogItemViewModel m_selectedEvent{ nullptr };
        ::AstralChronicle::services::QueryCancellation m_detailsCancellation;
        ::AstralChronicle::viewmodels::EventItemSettings m_eventItemSettings;
        bool m_autoScroll{ true };
        bool m_groupRepeated{};
        bool m_redactComputerName{};
        bool m_redactUserNames{};
        bool m_showCritical{ true };
        bool m_showErrors{ true };
        bool m_showWarnings{ true };
        bool m_showInformation{ true };
        bool m_isRecording{};
        bool m_hasStatusMessage{ true };
        bool m_isRunning{};
        bool m_isPaused{};
        bool m_isDetailsLoading{};
        std::uint32_t m_droppedCount{};
        std::uint64_t m_totalReceived{};
        std::uint32_t m_criticalCount{};
        std::uint32_t m_errorCount{};
        std::uint32_t m_warningCount{};
        std::uint32_t m_queueDepth{};
        std::uint32_t m_bookmarkCount{};
        std::size_t m_maxVisibleRows{ 2'000 };
        std::uint64_t m_detailsRequestVersion{};
        std::uint64_t m_lifetimeVersion{};
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct LiveViewModel : LiveViewModelT<LiveViewModel, implementation::LiveViewModel>
    {
    };
}
