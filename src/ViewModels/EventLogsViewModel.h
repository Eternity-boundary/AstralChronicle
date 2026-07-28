// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "EventLogsViewModel.g.h"
#include "EventLogItemViewModel.h"
#include "Models/EventChannelDescriptor.h"
#include "Models/EventFilter.h"
#include "Models/SavedView.h"
#include "Services/IEventBookmarkStore.h"
#include "Services/IEventQueryService.h"
#include "Services/ISavedViewRepository.h"
#include "Services/ITextExportService.h"

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct EventLogsViewModel : EventLogsViewModelT<EventLogsViewModel>
    {
        EventLogsViewModel();
        ~EventLogsViewModel() noexcept;

        void Initialize(
            std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
            std::shared_ptr<::AstralChronicle::services::IEventBookmarkStore> bookmarkStore,
            std::shared_ptr<::AstralChronicle::services::ITextExportService> textExporter,
            std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> savedViews,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel = std::nullopt,
            std::optional<std::wstring> const& query = std::nullopt,
            std::optional<std::wstring> const& searchText = std::nullopt);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring ChannelPath() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] bool IsAccessDenied() const noexcept;
        [[nodiscard]] bool IsLoading() const noexcept;
        [[nodiscard]] winrt::hstring SearchText() const;
        void SearchText(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterProvider() const;
        void FilterProvider(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterEventId() const;
        void FilterEventId(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterLevel() const;
        void FilterLevel(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterTask() const;
        void FilterTask(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterOpcode() const;
        void FilterOpcode(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterKeyword() const;
        void FilterKeyword(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterProcessId() const;
        void FilterProcessId(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterThreadId() const;
        void FilterThreadId(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterUser() const;
        void FilterUser(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring FilterComputer() const;
        void FilterComputer(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring RawXPath() const;
        void RawXPath(winrt::hstring const& value);
        [[nodiscard]] bool RawXPathEnabled() const noexcept;
        [[nodiscard]] bool FilterAfterToday() const noexcept;
        void FilterAfterToday(bool value);
        [[nodiscard]] winrt::hstring FilterAfterHours() const;
        void FilterAfterHours(winrt::hstring const& value);
        [[nodiscard]] bool HasStructuredFilter() const noexcept;
        [[nodiscard]] winrt::hstring FilterSummary() const;
        [[nodiscard]] bool HasFilter() const noexcept;
        [[nodiscard]] bool ShowBookmarkedOnly() const noexcept;
        void ShowBookmarkedOnly(bool value);
        [[nodiscard]] std::uint32_t BookmarkCount() const noexcept;
        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] std::uint32_t SelectedCount() const noexcept;
        [[nodiscard]] winrt::hstring CopySelectedEventText() const;
        [[nodiscard]] winrt::hstring CopySelectedEventsText() const;
        void ToggleBookmarks();
        void ToggleBookmark(winrt::AstralChronicle::EventLogItemViewModel const& value);
        void ExportSelectedEvents(winrt::Microsoft::UI::WindowId const& windowId);
        [[nodiscard]] bool SaveCurrentView(bool detailsPaneOpen);
        [[nodiscard]] winrt::hstring SortKey() const;
        [[nodiscard]] bool SortAscending() const noexcept;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> Events() const;
        [[nodiscard]] winrt::AstralChronicle::EventLogItemViewModel SelectedEvent() const;
        void SelectedEvent(winrt::AstralChronicle::EventLogItemViewModel const& value);
        void SetSelectedEvents(
            std::vector<winrt::AstralChronicle::EventLogItemViewModel> const& values,
            winrt::AstralChronicle::EventLogItemViewModel const& preferred = nullptr);
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
        [[nodiscard]] bool CanCloseActiveContext() const noexcept;
        void OpenSavedLog(std::wstring filePath);
        void CloseActiveContext();
        void Refresh();
        void LoadMore();
        void ClearFilter();
        void ApplyStructuredFilter();
        void ClearStructuredFilter();
        void SortBy(winrt::hstring const& key);

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget LoadAsync(
            std::uint64_t requestVersion,
            std::wstring channel,
            std::wstring query,
            std::uint32_t skippedRecords,
            std::uint32_t maximumRecords,
            bool append,
            bool savedLogFile,
            ::AstralChronicle::services::QueryCancellation cancellation);
        winrt::fire_and_forget LoadDetailsAsync(
            std::uint64_t requestVersion,
            std::wstring channel,
            std::uint64_t recordId,
            bool savedLogFile,
            ::AstralChronicle::services::QueryCancellation cancellation);
        winrt::fire_and_forget ExportSelectedEventsAsync(
            std::wstring text,
            std::uint64_t requestVersion,
            winrt::Microsoft::UI::WindowId windowId);
        void ApplyResult(::AstralChronicle::services::EventQueryResult const& result, bool append);
        void ApplyDetails(::AstralChronicle::services::EventDetailsResult const& result);
        void ApplyFilter();
        void SetBookmarkState(
            winrt::AstralChronicle::EventLogItemViewModel const& value,
            bool isBookmarked);
        void ClearSelection();
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();
        void RaiseSelectionProperties();
        void RaiseFilterProperties();

        struct CloseTargetContext final
        {
            std::wstring Heading;
            std::wstring ChannelPath;
            std::wstring BaseQuery{ L"*" };
            bool IsStructuredQuery{};
            std::uint32_t PageSize{ 256 };
        };

        std::shared_ptr<::AstralChronicle::services::IEventQueryService> m_eventQuery;
        std::shared_ptr<::AstralChronicle::services::IEventBookmarkStore> m_bookmarkStore;
        std::shared_ptr<::AstralChronicle::services::ITextExportService> m_textExporter;
        std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> m_savedViews;
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        std::wstring m_channelPath;
        bool m_isStructuredQuery{};
        bool m_isSavedLog{};
        CloseTargetContext m_closeTargetContext;
        std::wstring m_baseQuery{ L"*" };
        std::wstring m_query{ L"*" };
        std::optional<std::uint64_t> m_initialRecordId;
        std::uint64_t m_requestVersion{};
        std::uint64_t m_detailsRequestVersion{};
        std::uint32_t m_loadedRecordCount{};
        std::uint32_t m_pageSize{ 256 };
        std::uint32_t m_globalSearchTargetResultCount{};
        ::AstralChronicle::services::QueryCancellation m_cancellation;
        ::AstralChronicle::services::QueryCancellation m_detailsCancellation;
        ::AstralChronicle::viewmodels::EventItemSettings m_eventItemSettings;
        std::uint32_t m_queryBatchSize{ 256 };
        winrt::hstring m_heading;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        winrt::hstring m_searchText;
        std::wstring m_globalSearchText;
        winrt::hstring m_filterProvider;
        winrt::hstring m_filterEventId;
        winrt::hstring m_filterLevel{ L"Any" };
        winrt::hstring m_filterTask;
        winrt::hstring m_filterOpcode;
        winrt::hstring m_filterKeyword;
        winrt::hstring m_filterProcessId;
        winrt::hstring m_filterThreadId;
        winrt::hstring m_filterUser;
        winrt::hstring m_filterComputer;
        winrt::hstring m_rawXPath;
        winrt::hstring m_filterAfterHours;
        winrt::hstring m_filterSummary;
        winrt::hstring m_sortKey{ L"Time" };
        bool m_sortAscending{ false };
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
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_allEvents{ nullptr };
        winrt::AstralChronicle::EventLogItemViewModel m_selectedEvent{ nullptr };
        std::vector<winrt::AstralChronicle::EventLogItemViewModel> m_selectedEvents;
        bool m_hasStatusMessage{ true };
        bool m_isAccessDenied{};
        bool m_isLoading{};
        bool m_hasMoreEvents{ true };
        bool m_isDetailsLoading{};
        bool m_isGlobalSearch{};
        bool m_canCloseActiveContext{};
        bool m_filterAfterToday{};
        bool m_hasStructuredFilter{};
        bool m_rawXPathEnabled{};
        bool m_showBookmarkedOnly{};
        std::unordered_set<std::wstring> m_bookmarkedKeys;
        std::unordered_set<std::wstring> m_loadedEventKeys;
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct EventLogsViewModel : EventLogsViewModelT<EventLogsViewModel, implementation::EventLogsViewModel>
    {
    };
}
