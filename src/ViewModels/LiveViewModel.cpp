// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "LiveViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "PersistedSettings.h"

#include "LiveViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::uint32_t DefaultQueueLimit = 5'000;
    constexpr std::uint32_t MaximumQueueLimit = 100'000;
    constexpr std::size_t MaximumRecordedEvents = 10'000;

    [[nodiscard]] std::int32_t ChannelIndexFromName(winrt::hstring const& channel) noexcept
    {
        if (channel == L"Application") return 1;
        if (channel == L"Security") return 2;
        if (channel == L"Setup") return 3;
        if (channel == L"ForwardedEvents") return 4;
        return 0;
    }

    [[nodiscard]] winrt::hstring ChannelNameFromIndex(std::int32_t const index)
    {
        switch (index)
        {
        case 0: return L"System";
        case 1: return L"Application";
        case 2: return L"Security";
        case 3: return L"Setup";
        case 4: return L"ForwardedEvents";
        default: return {};
        }
    }

    [[nodiscard]] std::uint32_t ParseQueueLimit(winrt::hstring const& value) noexcept
    {
        if (value.empty())
        {
            return DefaultQueueLimit;
        }

        std::uint64_t parsed{};
        for (auto const character : value)
        {
            if (character < L'0' || character > L'9')
            {
                return DefaultQueueLimit;
            }
            auto const digit = static_cast<std::uint32_t>(character - L'0');
            if (parsed > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10)
            {
                return MaximumQueueLimit;
            }
            parsed = parsed * 10 + digit;
        }
        return std::clamp(
            static_cast<std::uint32_t>((std::min)(parsed, static_cast<std::uint64_t>(MaximumQueueLimit))),
            1u,
            MaximumQueueLimit);
    }

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

}

namespace winrt::AstralChronicle::implementation
{
    struct LiveUpdateDispatchState final
    {
        std::atomic_bool Active{ true };
        std::atomic_bool Pending{};
    };

    LiveViewModel::LiveViewModel()
        : m_events(winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>())
    {
    }

    LiveViewModel::~LiveViewModel() noexcept
    {
        Shutdown();
    }

    void LiveViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventLiveService> liveService,
        std::shared_ptr<::AstralChronicle::services::IEventLiveDataService> liveEventData,
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        Shutdown();
        m_liveService = std::move(liveService);
        m_liveEventData = std::move(liveEventData);
        m_eventQuery = std::move(eventQuery);
        m_strings = std::move(strings);
        if (!m_liveService || !m_liveEventData || !m_strings)
        {
            throw std::invalid_argument("Live monitoring requires live, event data, and string services.");
        }
        m_dispatcher = dispatcher;
        auto const settings = ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load();
        m_queueLimit = winrt::to_hstring(settings.LiveQueueLimit);
        m_maxVisibleRows = settings.MaxVisibleLiveRows;
        m_groupRepeated = settings.GroupRepeatedEvents;
        m_redactComputerName = settings.EventItems.RedactComputerName;
        m_redactUserNames = settings.EventItems.RedactUserNames;
        m_eventItemSettings = settings.EventItems;
        m_heading = m_strings->GetString(L"Live.Heading");
        m_summary = m_strings->GetString(L"Live.Summary");
        m_stateText = m_strings->GetString(L"Live.State.Stopped");
        m_statusText = m_strings->GetString(L"Live.Ready.Text");
        m_statusDetails = m_strings->GetString(L"Live.ReadyDetails.Text");
        ClearSelection();
        m_liveUpdateDispatch = std::make_shared<LiveUpdateDispatchState>();
        auto const weakThis = get_weak();
        auto const updateDispatch = m_liveUpdateDispatch;
        m_liveService->SetBatchAvailableCallback([weakThis, dispatcher, updateDispatch]() noexcept
        {
            // Coalesce many Event Log callbacks into one UI-thread batch update.
            if (!updateDispatch->Active.load(std::memory_order_acquire) ||
                updateDispatch->Pending.exchange(true, std::memory_order_acq_rel))
            {
                return;
            }

            bool queued{};
            try
            {
                queued = dispatcher && dispatcher.TryEnqueue(
                    Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal,
                    [weakThis, updateDispatch]()
                    {
                        updateDispatch->Pending.store(false, std::memory_order_release);
                        if (!updateDispatch->Active.load(std::memory_order_acquire))
                        {
                            return;
                        }
                        if (auto const strongThis = weakThis.get())
                        {
                            strongThis->DrainAvailableEvents();
                        }
                    });
            }
            catch (...)
            {
            }
            if (!queued)
            {
                updateDispatch->Pending.store(false, std::memory_order_release);
            }
        });
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"QueueLimit");
        RaisePropertyChanged(L"GroupRepeated");
    }

    void LiveViewModel::Start()
    {
        if (!m_liveService || !m_strings) return;
        auto const queueLimit = ParseQueueLimit(m_queueLimit);
        auto const normalizedQueueLimit = winrt::to_hstring(queueLimit);
        if (m_queueLimit != normalizedQueueLimit)
        {
            m_queueLimit = normalizedQueueLimit;
            RaisePropertyChanged(L"QueueLimit");
        }
        m_stateText = m_strings->GetString(L"Live.State.Starting");
        RaisePropertyChanged(L"StateText");
        bool started{};
        try
        {
            started = m_liveService->Start(m_channel.c_str(), m_query.c_str(), queueLimit);
        }
        catch (...)
        {
            started = false;
        }
        m_isRunning = started;
        m_isPaused = false;
        if (started)
        {
            m_stateText = m_strings->GetString(L"Live.State.Running");
            SetStatus(
                m_strings->GetString(L"Live.Started.Text"),
                FormatResource(m_strings->GetString(L"Live.StartedDetails.Text"), { m_channel }),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        }
        else
        {
            m_stateText = m_strings->GetString(L"Live.State.Error");
            SetStatus(m_strings->GetString(L"Live.StartFailed.Text"), m_strings->GetString(L"Live.ErrorDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"IsPaused");
        RaisePropertyChanged(L"CanStart");
    }

    void LiveViewModel::Pause()
    {
        if (!m_liveService || !m_isRunning) return;
        m_liveService->Pause();
        m_isPaused = true;
        m_stateText = m_strings->GetString(L"Live.State.Paused");
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsPaused");
    }

    void LiveViewModel::Resume()
    {
        if (!m_liveService || !m_isPaused) return;
        m_liveService->Resume();
        m_isPaused = false;
        m_stateText = m_strings->GetString(L"Live.State.Running");
        DrainAvailableEvents();
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsPaused");
    }

    void LiveViewModel::Stop()
    {
        if (!m_liveService) return;
        m_liveService->Stop();
        m_isRunning = false;
        m_isPaused = false;
        m_stateText = m_strings->GetString(L"Live.State.Stopped");
        if (m_strings)
        {
            SetStatus(
                m_strings->GetString(L"Live.Stopped.Text"),
                m_strings->GetString(L"Live.StoppedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        }
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"IsPaused");
        RaisePropertyChanged(L"CanStart");
    }

    void LiveViewModel::Clear()
    {
        if (m_liveService) m_liveService->Clear();
        m_events = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_droppedCount = 0;
        m_totalReceived = 0;
        m_criticalCount = 0;
        m_errorCount = 0;
        m_warningCount = 0;
        m_queueDepth = 0;
        m_eventsPerSecond = L"0";
        m_duration = L"0s";
        m_allEvents.clear();
        m_recordedEvents.clear();
        m_bookmarkCount = 0;
        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        ++m_detailsRequestVersion;
        ClearSelection();
        RaisePropertyChanged(L"Events");
        RaiseMetricProperties();
        RaisePropertyChanged(L"SelectedEvent");
        RaisePropertyChanged(L"HasSelection");
        RaiseSelectionProperties();
    }

    void LiveViewModel::DrainAvailableEvents()
    {
        if (!m_liveService) return;
        try
        {
            auto batch = m_liveService->TakeBatch(m_isPaused ? 0u : 250u);
            ApplyBatch(std::move(batch));
        }
        catch (...)
        {
            Stop();
            if (m_strings)
            {
                SetStatus(
                    m_strings->GetString(L"Live.StreamFailed.Text"),
                    m_strings->GetString(L"Live.ErrorDetails.Text"),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            }
        }
    }

    void LiveViewModel::ApplyBatch(::AstralChronicle::services::EventLiveBatch batch)
    {
        if (m_isRecording && !batch.Events.empty())
        {
            auto const incomingCount = batch.Events.size();
            if (incomingCount >= MaximumRecordedEvents)
            {
                m_recordedEvents.clear();
                auto const first = batch.Events.end() -
                    static_cast<std::ptrdiff_t>(MaximumRecordedEvents);
                for (auto iterator = first; iterator != batch.Events.end(); ++iterator)
                {
                    m_recordedEvents.push_back(iterator->RawXml);
                }
            }
            else
            {
                auto const required = m_recordedEvents.size() + incomingCount;
                if (required > MaximumRecordedEvents)
                {
                    auto const removeCount = required - MaximumRecordedEvents;
                    m_recordedEvents.erase(
                        m_recordedEvents.begin(),
                        m_recordedEvents.begin() + static_cast<std::ptrdiff_t>(removeCount));
                }
                for (auto const& record : batch.Events)
                {
                    m_recordedEvents.push_back(record.RawXml);
                }
            }
        }

        auto rebuildRequired = m_groupRepeated;
        for (auto& record : batch.Events)
        {
            rebuildRequired = AppendEvent(std::move(record)) || rebuildRequired;
        }
        if (rebuildRequired)
        {
            RebuildEventView();
            RaisePropertyChanged(L"Events");
        }

        m_droppedCount = batch.DroppedCount;
        m_totalReceived = batch.TotalReceived;
        m_criticalCount = batch.CriticalCount;
        m_errorCount = batch.ErrorCount;
        m_warningCount = batch.WarningCount;
        m_queueDepth = batch.QueueDepth;
        auto const seconds = std::max<std::int64_t>(1, batch.Duration.count() / 1000);
        m_eventsPerSecond = winrt::to_hstring(m_totalReceived / static_cast<std::uint64_t>(seconds));
        m_duration = winrt::to_hstring(seconds) + L"s";
        if (batch.State == ::AstralChronicle::services::LiveState::Error)
        {
            m_isRunning = false;
            m_isPaused = false;
            m_stateText = m_strings->GetString(L"Live.State.Error");
            SetStatus(m_strings->GetString(L"Live.StreamFailed.Text"),
                m_strings->GetString(L"Live.ErrorDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
        else if (batch.State == ::AstralChronicle::services::LiveState::EventsLost)
        {
            m_stateText = m_strings->GetString(L"Live.State.EventsLost");
            SetStatus(m_strings->GetString(L"Live.EventsLost.Text"),
                m_strings->GetString(L"Live.EventsLostDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
        }
        RaiseMetricProperties();
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"CanStart");
    }

    void LiveViewModel::RebuildEventView()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        std::wstring previousXml;
        for (auto const& event : m_allEvents)
        {
            if (!MatchesLevelFilter(event.Record.Summary.Level))
            {
                continue;
            }
            if (m_groupRepeated && !previousXml.empty() && previousXml == event.Record.RawXml)
            {
                continue;
            }
            values.Append(event.Item);
            previousXml = event.Record.RawXml;
        }
        m_events = values;

        if (m_selectedEvent)
        {
            auto const selectedStillVisible = std::any_of(m_events.begin(), m_events.end(), [this](auto const& item)
            {
                return item == m_selectedEvent;
            });
            if (!selectedStillVisible)
            {
                SelectedEvent(nullptr);
            }
        }
    }

    bool LiveViewModel::AppendEvent(::AstralChronicle::models::LiveEventRecord record)
    {
        auto const level = record.Summary.Level;
        auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
        winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
            record.Summary,
            *m_strings,
            m_eventItemSettings);
        m_allEvents.push_back({ item, std::move(record) });

        auto requiresRebuild = m_groupRepeated;
        while (m_allEvents.size() > m_maxVisibleRows)
        {
            auto const removedItem = m_allEvents.front().Item;
            m_allEvents.erase(m_allEvents.begin());
            requiresRebuild = true;
            if (removedItem == m_selectedEvent)
            {
                SelectedEvent(nullptr);
            }
        }

        if (!requiresRebuild && MatchesLevelFilter(level))
        {
            m_events.Append(item);
        }
        return requiresRebuild;
    }

    bool LiveViewModel::MatchesLevelFilter(std::uint8_t const level) const noexcept
    {
        if (level == 1) return m_showCritical;
        if (level == 2) return m_showErrors;
        if (level == 3) return m_showWarnings;
        return m_showInformation;
    }

    LiveEventEntry const* LiveViewModel::FindEvent(
        winrt::AstralChronicle::EventLogItemViewModel const& item) const noexcept
    {
        auto const iterator = std::find_if(m_allEvents.begin(), m_allEvents.end(), [&item](LiveEventEntry const& event)
        {
            return event.Item == item;
        });
        return iterator == m_allEvents.end() ? nullptr : &*iterator;
    }

    void LiveViewModel::SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity const severity)
    {
        m_statusText = std::move(title);
        m_statusDetails = std::move(details);
        m_statusSeverity = severity;
        m_hasStatusMessage = true;
        RaiseStatusProperties();
    }

    winrt::hstring LiveViewModel::Heading() const { return m_heading; }
    winrt::hstring LiveViewModel::Summary() const { return m_summary; }
    winrt::hstring LiveViewModel::StatusText() const { return m_statusText; }
    winrt::hstring LiveViewModel::StatusDetails() const { return m_statusDetails; }
    winrt::hstring LiveViewModel::StateText() const { return m_stateText; }
    winrt::hstring LiveViewModel::Channel() const { return m_channel; }
    void LiveViewModel::Channel(winrt::hstring const& value)
    {
        if (m_channel == value) return;
        m_channel = value;
        RaisePropertyChanged(L"Channel");
        RaisePropertyChanged(L"ChannelIndex");
    }
    std::int32_t LiveViewModel::ChannelIndex() const noexcept { return ChannelIndexFromName(m_channel); }
    void LiveViewModel::ChannelIndex(std::int32_t const value)
    {
        auto const channel = ChannelNameFromIndex(value);
        if (!channel.empty()) Channel(channel);
    }
    winrt::hstring LiveViewModel::Query() const { return m_query; }
    void LiveViewModel::Query(winrt::hstring const& value) { m_query = value; RaisePropertyChanged(L"Query"); }
    winrt::hstring LiveViewModel::QueueLimit() const { return m_queueLimit; }
    void LiveViewModel::QueueLimit(winrt::hstring const& value) { m_queueLimit = value; RaisePropertyChanged(L"QueueLimit"); }
    bool LiveViewModel::AutoScroll() const noexcept { return m_autoScroll; }
    void LiveViewModel::AutoScroll(bool const value) { m_autoScroll = value; RaisePropertyChanged(L"AutoScroll"); }
    bool LiveViewModel::GroupRepeated() const noexcept { return m_groupRepeated; }
    void LiveViewModel::GroupRepeated(bool const value)
    {
        if (m_groupRepeated == value) return;
        m_groupRepeated = value;
        RebuildEventView();
        RaisePropertyChanged(L"GroupRepeated");
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"EventCount");
    }
    bool LiveViewModel::ShowCritical() const noexcept { return m_showCritical; }
    void LiveViewModel::ShowCritical(bool const value) { m_showCritical = value; RebuildEventView(); RaisePropertyChanged(L"ShowCritical"); RaisePropertyChanged(L"Events"); RaisePropertyChanged(L"EventCount"); }
    bool LiveViewModel::ShowErrors() const noexcept { return m_showErrors; }
    void LiveViewModel::ShowErrors(bool const value) { m_showErrors = value; RebuildEventView(); RaisePropertyChanged(L"ShowErrors"); RaisePropertyChanged(L"Events"); RaisePropertyChanged(L"EventCount"); }
    bool LiveViewModel::ShowWarnings() const noexcept { return m_showWarnings; }
    void LiveViewModel::ShowWarnings(bool const value) { m_showWarnings = value; RebuildEventView(); RaisePropertyChanged(L"ShowWarnings"); RaisePropertyChanged(L"Events"); RaisePropertyChanged(L"EventCount"); }
    bool LiveViewModel::ShowInformation() const noexcept { return m_showInformation; }
    void LiveViewModel::ShowInformation(bool const value) { m_showInformation = value; RebuildEventView(); RaisePropertyChanged(L"ShowInformation"); RaisePropertyChanged(L"Events"); RaisePropertyChanged(L"EventCount"); }
    bool LiveViewModel::IsRecording() const noexcept { return m_isRecording; }
    winrt::hstring LiveViewModel::EventsPerSecond() const { return m_eventsPerSecond; }
    std::uint64_t LiveViewModel::TotalReceived() const noexcept { return m_totalReceived; }
    std::uint32_t LiveViewModel::CriticalCount() const noexcept { return m_criticalCount; }
    std::uint32_t LiveViewModel::ErrorCount() const noexcept { return m_errorCount; }
    std::uint32_t LiveViewModel::WarningCount() const noexcept { return m_warningCount; }
    std::uint32_t LiveViewModel::QueueDepth() const noexcept { return m_queueDepth; }
    winrt::hstring LiveViewModel::Duration() const { return m_duration; }
    std::uint32_t LiveViewModel::BookmarkCount() const noexcept { return m_bookmarkCount; }
    bool LiveViewModel::IsRunning() const noexcept { return m_isRunning; }
    bool LiveViewModel::IsPaused() const noexcept { return m_isPaused; }
    bool LiveViewModel::CanStart() const noexcept { return !m_isRunning; }
    std::uint32_t LiveViewModel::DroppedCount() const noexcept { return m_droppedCount; }
    std::uint32_t LiveViewModel::EventCount() const noexcept { return m_events ? m_events.Size() : 0; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> LiveViewModel::Events() const { return m_events; }

    winrt::AstralChronicle::EventLogItemViewModel LiveViewModel::SelectedEvent() const
    {
        return m_selectedEvent;
    }

    void LiveViewModel::SelectedEvent(winrt::AstralChronicle::EventLogItemViewModel const& value)
    {
        if (m_selectedEvent == value)
        {
            return;
        }

        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        ++m_detailsRequestVersion;
        m_selectedEvent = value;
        if (!m_selectedEvent)
        {
            ClearSelection();
            RaisePropertyChanged(L"SelectedEvent");
            RaisePropertyChanged(L"HasSelection");
            RaiseSelectionProperties();
            return;
        }

        auto const event = FindEvent(m_selectedEvent);
        if (!event)
        {
            ClearSelection();
            RaisePropertyChanged(L"SelectedEvent");
            RaisePropertyChanged(L"HasSelection");
            RaiseSelectionProperties();
            return;
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
        m_selectedMessage = m_selectedDescription;
        m_selectedXml = winrt::hstring{ event->Record.RawXml };

        auto const empty = m_strings ? m_strings->GetString(L"EventLogs.EmptyValue.Text") : winrt::hstring{};
        m_selectedEventData = empty;
        m_selectedUserData = empty;
        m_selectedProviderMetadata = empty;
        m_selectedBinaryData = empty;
        m_selectedRelatedEvents = empty;

        auto const recordId = m_selectedEvent.SortRecordId();
        auto selectedChannel = std::wstring{ m_selectedEvent.Channel().c_str() };
        if (selectedChannel.empty() || selectedChannel == std::wstring{ empty.c_str() })
        {
            selectedChannel = std::wstring{ m_channel.c_str() };
        }
        if (m_liveEventData)
        {
            m_isDetailsLoading = true;
            m_detailsStatusText = m_strings->GetString(L"EventLogs.DetailsLoading.Text");
            m_detailsCancellation = ::AstralChronicle::services::MakeQueryCancellation();
            auto const requestVersion = m_detailsRequestVersion;
            LoadDetailsAsync(
                requestVersion,
                std::move(selectedChannel),
                recordId,
                event->Record,
                m_detailsCancellation);
        }
        else
        {
            m_isDetailsLoading = false;
            m_detailsStatusText = m_strings
                ? m_strings->GetString(L"EventLogs.DetailsLoaded.Text")
                : winrt::hstring{};
        }

        RaisePropertyChanged(L"SelectedEvent");
        RaisePropertyChanged(L"HasSelection");
        RaiseSelectionProperties();
    }

    bool LiveViewModel::HasSelection() const noexcept { return static_cast<bool>(m_selectedEvent); }
    winrt::hstring LiveViewModel::SelectedProvider() const { return m_selectedProvider; }
    winrt::hstring LiveViewModel::SelectedEventId() const { return m_selectedEventId; }
    winrt::hstring LiveViewModel::SelectedVersion() const { return m_selectedVersion; }
    winrt::hstring LiveViewModel::SelectedLevel() const { return m_selectedLevel; }
    winrt::hstring LiveViewModel::SelectedOpcode() const { return m_selectedOpcode; }
    winrt::hstring LiveViewModel::SelectedKeywords() const { return m_selectedKeywords; }
    winrt::hstring LiveViewModel::SelectedTimeCreated() const { return m_selectedTimeCreated; }
    winrt::hstring LiveViewModel::SelectedTaskCategory() const { return m_selectedTaskCategory; }
    winrt::hstring LiveViewModel::SelectedChannel() const { return m_selectedChannel; }
    winrt::hstring LiveViewModel::SelectedUser() const { return m_selectedUser; }
    winrt::hstring LiveViewModel::SelectedComputer() const { return m_selectedComputer; }
    winrt::hstring LiveViewModel::SelectedRecordId() const { return m_selectedRecordId; }
    winrt::hstring LiveViewModel::SelectedProcessId() const { return m_selectedProcessId; }
    winrt::hstring LiveViewModel::SelectedThreadId() const { return m_selectedThreadId; }
    winrt::hstring LiveViewModel::SelectedActivityId() const { return m_selectedActivityId; }
    winrt::hstring LiveViewModel::SelectedRelatedActivityId() const { return m_selectedRelatedActivityId; }
    winrt::hstring LiveViewModel::SelectedDescription() const { return m_selectedDescription; }
    winrt::hstring LiveViewModel::SelectedMessage() const { return m_selectedMessage; }
    winrt::hstring LiveViewModel::SelectedXml() const { return m_selectedXml; }
    winrt::hstring LiveViewModel::SelectedEventData() const { return m_selectedEventData; }
    winrt::hstring LiveViewModel::SelectedUserData() const { return m_selectedUserData; }
    winrt::hstring LiveViewModel::SelectedProviderMetadata() const { return m_selectedProviderMetadata; }
    winrt::hstring LiveViewModel::SelectedBinaryData() const { return m_selectedBinaryData; }
    winrt::hstring LiveViewModel::SelectedRelatedEvents() const { return m_selectedRelatedEvents; }
    winrt::hstring LiveViewModel::DetailsStatusText() const { return m_detailsStatusText; }
    bool LiveViewModel::IsDetailsLoading() const noexcept { return m_isDetailsLoading; }

    winrt::fire_and_forget LiveViewModel::LoadDetailsAsync(
        std::uint64_t const requestVersion,
        std::wstring channel,
        std::uint64_t const recordId,
        ::AstralChronicle::models::LiveEventRecord record,
        ::AstralChronicle::services::QueryCancellation cancellation)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const eventQuery = m_eventQuery;
            auto const eventData = m_liveEventData;
            auto const dispatcher = m_dispatcher;
            if (!eventData || !dispatcher) co_return;
            co_await winrt::resume_background();
            ::AstralChronicle::models::EventDetails details;
            auto querySucceeded = false;
            if (eventQuery && recordId != 0 && !channel.empty())
            {
                auto result = eventQuery->QueryDetails(channel, recordId, cancellation);
                if (result.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded)
                {
                    details = std::move(result.Details);
                    querySucceeded = true;
                }
            }
            if (cancellation && cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            if (!querySucceeded)
            {
                details = eventData->CreateFallbackDetails(record);
            }
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || requestVersion != strongThis->m_detailsRequestVersion ||
                cancellation != strongThis->m_detailsCancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            strongThis->ApplyDetails(details, querySucceeded);
        }
        catch (...)
        {
            co_return;
        }
    }

    void LiveViewModel::ApplyDetails(
        ::AstralChronicle::models::EventDetails const& details,
        bool const querySucceeded)
    {
        m_isDetailsLoading = false;
        if (!m_strings || !m_liveEventData)
        {
            return;
        }

        auto const empty = m_strings->GetString(L"EventLogs.EmptyValue.Text");
        auto const detailText = m_liveEventData->CreateDetailsText(details);
        if (!detailText.Message.empty())
        {
            m_selectedMessage = winrt::hstring{ detailText.Message };
        }
        if (m_selectedXml.empty() && !detailText.RawXml.empty())
        {
            m_selectedXml = winrt::hstring{ detailText.RawXml };
        }
        if (!detailText.EventData.empty()) m_selectedEventData = winrt::hstring{ detailText.EventData };
        if (!detailText.UserData.empty()) m_selectedUserData = winrt::hstring{ detailText.UserData };
        m_selectedProviderMetadata = detailText.ProviderMetadata.empty()
            ? empty
            : winrt::hstring{ detailText.ProviderMetadata };
        if (!detailText.BinaryData.empty()) m_selectedBinaryData = winrt::hstring{ detailText.BinaryData };
        if (!detailText.RelatedEvents.empty()) m_selectedRelatedEvents = winrt::hstring{ detailText.RelatedEvents };

        if (querySucceeded)
        {
            m_detailsStatusText = details.FormattingErrorCode == 0
                ? m_strings->GetString(L"EventLogs.DetailsLoaded.Text")
                : FormatResource(
                    m_strings->GetString(L"EventLogs.MessageFormattingError.Text"),
                    { winrt::to_hstring(details.FormattingErrorCode) });
        }
        else
        {
            m_detailsStatusText = m_strings->GetString(L"EventLogs.MessageFormattingFailed.Text");
        }

        RaiseSelectionProperties();
    }

    void LiveViewModel::ClearSelection()
    {
        m_selectedEvent = nullptr;
        auto const empty = m_strings ? m_strings->GetString(L"EventLogs.EmptyValue.Text") : winrt::hstring{};
        auto const selectEvent = m_strings ? m_strings->GetString(L"EventLogs.SelectEvent.Text") : winrt::hstring{};
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
        m_selectedDescription = selectEvent;
        m_selectedMessage = selectEvent;
        m_selectedXml = selectEvent;
        m_selectedEventData = selectEvent;
        m_selectedUserData = selectEvent;
        m_selectedProviderMetadata = selectEvent;
        m_selectedBinaryData = selectEvent;
        m_selectedRelatedEvents = selectEvent;
        m_detailsStatusText = selectEvent;
        m_isDetailsLoading = false;
    }

    Microsoft::UI::Xaml::Controls::InfoBarSeverity LiveViewModel::StatusSeverity() const noexcept { return m_statusSeverity; }
    bool LiveViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    void LiveViewModel::ToggleRecording()
    {
        m_isRecording = !m_isRecording;
        if (m_isRecording) m_recordedEvents.clear();
        RaisePropertyChanged(L"IsRecording");
    }

    void LiveViewModel::Export()
    {
        if (!m_strings || m_recordedEvents.empty())
        {
            if (m_strings) SetStatus(
                m_strings->GetString(L"Live.ExportEmpty.Text"),
                m_strings->GetString(L"Live.ExportEmptyDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
            return;
        }
        ExportAsync(
            m_recordedEvents,
            m_lifetimeVersion,
            m_redactComputerName,
            m_redactUserNames);
    }

    void LiveViewModel::BookmarkLatest()
    {
        if (!m_events || m_events.Size() == 0) return;
        ++m_bookmarkCount;
        RaisePropertyChanged(L"BookmarkCount");
    }

    winrt::fire_and_forget LiveViewModel::ExportAsync(
        std::vector<std::wstring> events,
        std::uint64_t const lifetimeVersion,
        bool const redactComputerName,
        bool const redactUserNames)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            auto const strings = m_strings;
            auto const eventData = m_liveEventData;
            if (!dispatcher || !strings || !eventData) co_return;
            co_await winrt::resume_background();
            auto const exportResult = eventData->ExportRecordedEvents(
                std::move(events),
                redactComputerName,
                redactUserNames);

            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || lifetimeVersion != strongThis->m_lifetimeVersion || !strongThis->m_liveService || !strings)
            {
                co_return;
            }
            if (exportResult.ErrorCode == 0)
            {
                strongThis->SetStatus(
                    strings->GetString(L"Live.ExportCompleted.Text"),
                    FormatResource(
                        strings->GetString(L"Live.ExportCompletedDetails.Text"),
                        { winrt::hstring{ exportResult.Path } }),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            }
            else
            {
                strongThis->SetStatus(
                    strings->GetString(L"Live.ExportFailed.Text"),
                    FormatResource(
                        strings->GetString(L"Live.ErrorDetails.Text"),
                        { winrt::to_hstring(exportResult.ErrorCode) }),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            }
        }
        catch (...)
        {
            co_return;
        }
    }

    void LiveViewModel::Shutdown() noexcept
    {
        ++m_lifetimeVersion;
        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        ++m_detailsRequestVersion;
        if (m_liveUpdateDispatch)
        {
            m_liveUpdateDispatch->Active.store(false, std::memory_order_release);
        }
        auto const liveService = std::move(m_liveService);
        if (liveService)
        {
            liveService->SetBatchAvailableCallback({});
            liveService->Stop();
        }
        m_liveUpdateDispatch.reset();
        m_eventQuery.reset();
        m_liveEventData.reset();
        m_isRunning = false;
        m_isPaused = false;
        m_strings.reset();
        m_dispatcher = nullptr;
    }

    void LiveViewModel::RaiseMetricProperties()
    {
        RaisePropertyChanged(L"DroppedCount");
        RaisePropertyChanged(L"EventCount");
        RaisePropertyChanged(L"EventsPerSecond");
        RaisePropertyChanged(L"TotalReceived");
        RaisePropertyChanged(L"CriticalCount");
        RaisePropertyChanged(L"ErrorCount");
        RaisePropertyChanged(L"WarningCount");
        RaisePropertyChanged(L"QueueDepth");
        RaisePropertyChanged(L"Duration");
        RaisePropertyChanged(L"BookmarkCount");
    }
    void LiveViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText"); RaisePropertyChanged(L"StatusDetails"); RaisePropertyChanged(L"StatusSeverity"); RaisePropertyChanged(L"HasStatusMessage");
    }
    void LiveViewModel::RaiseSelectionProperties()
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
    winrt::event_token LiveViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_propertyChanged.add(handler); }
    void LiveViewModel::PropertyChanged(winrt::event_token const& token) noexcept { m_propertyChanged.remove(token); }
    void LiveViewModel::RaisePropertyChanged(winrt::hstring const& propertyName) { m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName }); }
}
