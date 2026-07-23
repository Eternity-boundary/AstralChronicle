// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "LiveViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "LiveViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <string>

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
}

namespace winrt::AstralChronicle::implementation
{
    LiveViewModel::LiveViewModel()
        : m_events(winrt::single_threaded_observable_vector<winrt::hstring>())
    {
    }

    void LiveViewModel::Initialize(
        ::AstralChronicle::services::IEventLiveService& liveService,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_liveService = &liveService;
        m_strings = &strings;
        m_dispatcher = dispatcher;
        m_heading = strings.GetString(L"Live.Heading");
        m_summary = strings.GetString(L"Live.Summary");
        m_stateText = strings.GetString(L"Live.State.Stopped");
        m_statusText = strings.GetString(L"Live.Ready.Text");
        m_statusDetails = strings.GetString(L"Live.ReadyDetails.Text");
        m_timer = dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::milliseconds{ 500 });
        m_timer.Tick([this](auto const&, auto const&) { OnTimerTick(); });
        RaisePropertyChanged(L"StateText");
    }

    void LiveViewModel::Start()
    {
        if (!m_liveService || !m_strings) return;
        std::uint32_t queueLimit{ 5000 };
        try { queueLimit = static_cast<std::uint32_t>(std::stoul(std::wstring{ m_queueLimit.c_str() })); } catch (...) {}
        auto const started = m_liveService->Start(m_channel.c_str(), m_query.c_str(), queueLimit);
        m_stateText = m_strings->GetString(L"Live.State.Starting");
        RaisePropertyChanged(L"StateText");
        m_isRunning = started;
        m_isPaused = false;
        if (started)
        {
            m_stateText = m_strings->GetString(L"Live.State.Running");
            SetStatus(m_strings->GetString(L"Live.Started.Text"), m_strings->GetString(L"Live.StartedDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            m_timer.Start();
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
        if (m_timer) m_timer.Stop();
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"IsPaused");
        RaisePropertyChanged(L"CanStart");
    }

    void LiveViewModel::Clear()
    {
        if (m_liveService) m_liveService->Clear();
        m_events = winrt::single_threaded_observable_vector<winrt::hstring>();
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
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"DroppedCount");
        RaisePropertyChanged(L"EventCount");
    }

    void LiveViewModel::OnTimerTick()
    {
        if (!m_liveService) return;
        auto const batch = m_liveService->TakeBatch(m_isPaused ? 0u : 250u);
        ApplyBatch(batch);
    }

    void LiveViewModel::ApplyBatch(::AstralChronicle::services::LiveBatch const& batch)
    {
        m_allEvents.insert(m_allEvents.end(), batch.Events.begin(), batch.Events.end());
        while (m_allEvents.size() > 2000) m_allEvents.erase(m_allEvents.begin());
        RebuildEventView();

        m_droppedCount += batch.DroppedCount;
        if (m_isRecording)
        {
            m_recordedEvents.insert(m_recordedEvents.end(), batch.Events.begin(), batch.Events.end());
        }
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
            if (m_timer) m_timer.Stop();
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
        RaisePropertyChanged(L"Events");
        RaisePropertyChanged(L"DroppedCount");
        RaisePropertyChanged(L"EventCount");
        RaisePropertyChanged(L"EventsPerSecond");
        RaisePropertyChanged(L"TotalReceived");
        RaisePropertyChanged(L"CriticalCount");
        RaisePropertyChanged(L"ErrorCount");
        RaisePropertyChanged(L"WarningCount");
        RaisePropertyChanged(L"QueueDepth");
        RaisePropertyChanged(L"Duration");
        RaisePropertyChanged(L"StateText");
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"CanStart");
    }

    void LiveViewModel::RebuildEventView()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& event : m_allEvents)
        {
            auto const levelStart = event.find(L"<Level>");
            auto const levelEnd = event.find(L"</Level>", levelStart);
            auto const level = levelStart == std::wstring::npos || levelEnd == std::wstring::npos
                ? std::wstring{}
                : event.substr(levelStart + 7, levelEnd - levelStart - 7);
            if ((level == L"1" && !m_showCritical) ||
                (level == L"2" && !m_showErrors) ||
                (level == L"3" && !m_showWarnings) ||
                (level != L"1" && level != L"2" && level != L"3" && !m_showInformation))
            {
                continue;
            }
            if (m_groupRepeated && values.Size() > 0 && values.GetAt(values.Size() - 1) == event)
            {
                continue;
            }
            values.Append(winrt::hstring{ event });
        }
        while (values.Size() > 2000) values.RemoveAt(0);
        m_events = values;
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
    void LiveViewModel::Channel(winrt::hstring const& value) { m_channel = value; RaisePropertyChanged(L"Channel"); }
    winrt::hstring LiveViewModel::Query() const { return m_query; }
    void LiveViewModel::Query(winrt::hstring const& value) { m_query = value; RaisePropertyChanged(L"Query"); }
    winrt::hstring LiveViewModel::QueueLimit() const { return m_queueLimit; }
    void LiveViewModel::QueueLimit(winrt::hstring const& value) { m_queueLimit = value; RaisePropertyChanged(L"QueueLimit"); }
    bool LiveViewModel::AutoScroll() const noexcept { return m_autoScroll; }
    void LiveViewModel::AutoScroll(bool const value) { m_autoScroll = value; RaisePropertyChanged(L"AutoScroll"); }
    bool LiveViewModel::GroupRepeated() const noexcept { return m_groupRepeated; }
    void LiveViewModel::GroupRepeated(bool const value) { m_groupRepeated = value; RaisePropertyChanged(L"GroupRepeated"); }
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
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> LiveViewModel::Events() const { return m_events; }
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
        ExportAsync(m_recordedEvents);
    }

    void LiveViewModel::BookmarkLatest()
    {
        if (!m_events || m_events.Size() == 0) return;
        ++m_bookmarkCount;
        RaisePropertyChanged(L"BookmarkCount");
    }

    winrt::fire_and_forget LiveViewModel::ExportAsync(std::vector<std::wstring> events)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        auto strings = m_strings;
        co_await winrt::resume_background();
        std::uint32_t errorCode{};
        winrt::hstring path;
        try
        {
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = co_await folder.CreateFileAsync(
                L"Live-recording.txt",
                winrt::Windows::Storage::CreationCollisionOption::GenerateUniqueName);
            std::wstring content;
            for (auto const& event : events) { content += event; content += L"\r\n"; }
            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, content);
            path = file.Path();
        }
        catch (winrt::hresult_error const& error) { errorCode = static_cast<std::uint32_t>(error.code().value); }
        co_await wil::resume_foreground(dispatcher);
        if (errorCode == 0)
        {
            SetStatus(
                strings->GetString(L"Live.ExportCompleted.Text"),
                FormatResource(strings->GetString(L"Live.ExportCompletedDetails.Text"), { path }),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        }
        else
        {
            SetStatus(
                strings->GetString(L"Live.ExportFailed.Text"),
                FormatResource(strings->GetString(L"Live.ErrorDetails.Text"), { winrt::to_hstring(errorCode) }),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
    }
    void LiveViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText"); RaisePropertyChanged(L"StatusDetails"); RaisePropertyChanged(L"StatusSeverity"); RaisePropertyChanged(L"HasStatusMessage");
    }
    winrt::event_token LiveViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_propertyChanged.add(handler); }
    void LiveViewModel::PropertyChanged(winrt::event_token const& token) noexcept { m_propertyChanged.remove(token); }
    void LiveViewModel::RaisePropertyChanged(winrt::hstring const& propertyName) { m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName }); }
}
