// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "LiveViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "PersistedSettings.h"

#include "LiveViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    constexpr std::uint32_t DefaultQueueLimit = 5'000;
    constexpr std::uint32_t MaximumQueueLimit = 100'000;
    constexpr std::size_t MaximumRecordedEvents = 10'000;

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

    void RedactXmlElement(std::wstring& xml, std::wstring_view const elementName)
    {
        auto const openingPrefix = L"<" + std::wstring{ elementName };
        auto const closingTag = L"</" + std::wstring{ elementName } + L">";
        std::size_t searchFrom{};
        while ((searchFrom = xml.find(openingPrefix, searchFrom)) != std::wstring::npos)
        {
            auto const boundary = searchFrom + openingPrefix.size();
            if (boundary >= xml.size() ||
                (xml[boundary] != L'>' && !std::iswspace(xml[boundary])))
            {
                searchFrom = boundary;
                continue;
            }
            auto const openingEnd = xml.find(L'>', boundary);
            if (openingEnd == std::wstring::npos)
            {
                return;
            }
            if (openingEnd > searchFrom && xml[openingEnd - 1] == L'/')
            {
                searchFrom = openingEnd + 1;
                continue;
            }
            auto const closingStart = xml.find(closingTag, openingEnd + 1);
            if (closingStart == std::wstring::npos)
            {
                return;
            }
            xml.replace(openingEnd + 1, closingStart - openingEnd - 1, L"[redacted]");
            searchFrom = openingEnd + 1 + std::wstring_view{ L"[redacted]" }.size() +
                closingTag.size();
        }
    }

    void RedactXmlAttribute(std::wstring& xml, std::wstring_view const attributeName)
    {
        std::size_t searchFrom{};
        while ((searchFrom = xml.find(attributeName, searchFrom)) != std::wstring::npos)
        {
            auto const before = searchFrom == 0 ? L' ' : xml[searchFrom - 1];
            auto cursor = searchFrom + attributeName.size();
            if ((!std::iswspace(before) && before != L'<') ||
                (cursor < xml.size() &&
                    !std::iswspace(xml[cursor]) &&
                    xml[cursor] != L'='))
            {
                searchFrom = cursor;
                continue;
            }
            while (cursor < xml.size() && std::iswspace(xml[cursor])) ++cursor;
            if (cursor >= xml.size() || xml[cursor] != L'=')
            {
                searchFrom = cursor;
                continue;
            }
            ++cursor;
            while (cursor < xml.size() && std::iswspace(xml[cursor])) ++cursor;
            if (cursor >= xml.size() || (xml[cursor] != L'"' && xml[cursor] != L'\''))
            {
                searchFrom = cursor;
                continue;
            }
            auto const quote = xml[cursor++];
            auto const valueEnd = xml.find(quote, cursor);
            if (valueEnd == std::wstring::npos)
            {
                return;
            }
            xml.replace(cursor, valueEnd - cursor, L"[redacted]");
            searchFrom = cursor + std::wstring_view{ L"[redacted]" }.size();
        }
    }

    [[nodiscard]] std::optional<std::wstring> XmlAttribute(
        std::wstring_view const tag,
        std::wstring_view const attributeName)
    {
        auto position = tag.find(attributeName);
        while (position != std::wstring_view::npos)
        {
            auto const before = position == 0 ? L' ' : tag[position - 1];
            auto cursor = position + attributeName.size();
            if ((std::iswspace(before) || before == L'<') &&
                (cursor >= tag.size() || std::iswspace(tag[cursor]) || tag[cursor] == L'='))
            {
                while (cursor < tag.size() && std::iswspace(tag[cursor])) ++cursor;
                if (cursor < tag.size() && tag[cursor] == L'=')
                {
                    ++cursor;
                    while (cursor < tag.size() && std::iswspace(tag[cursor])) ++cursor;
                    if (cursor < tag.size() && (tag[cursor] == L'"' || tag[cursor] == L'\''))
                    {
                        auto const quote = tag[cursor++];
                        auto const end = tag.find(quote, cursor);
                        if (end != std::wstring_view::npos)
                        {
                            return std::wstring{ tag.substr(cursor, end - cursor) };
                        }
                    }
                }
            }
            position = tag.find(attributeName, position + attributeName.size());
        }
        return std::nullopt;
    }

    [[nodiscard]] bool IsSensitiveDataName(
        std::wstring value,
        bool const redactComputerName,
        bool const redactUserNames)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t const character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        if (redactComputerName &&
            (value.find(L"computername") != std::wstring::npos ||
                value.find(L"machinename") != std::wstring::npos ||
                value.find(L"workstation") != std::wstring::npos))
        {
            return true;
        }
        return redactUserNames &&
            (value.find(L"username") != std::wstring::npos ||
                value.find(L"accountname") != std::wstring::npos ||
                value.find(L"membername") != std::wstring::npos ||
                value.find(L"userid") != std::wstring::npos);
    }

    void RedactNamedEventData(
        std::wstring& xml,
        bool const redactComputerName,
        bool const redactUserNames)
    {
        std::size_t searchFrom{};
        while ((searchFrom = xml.find(L"<Data", searchFrom)) != std::wstring::npos)
        {
            auto const boundary = searchFrom + 5;
            if (boundary >= xml.size() ||
                (xml[boundary] != L'>' && !std::iswspace(xml[boundary])))
            {
                searchFrom = boundary;
                continue;
            }
            auto const openingEnd = xml.find(L'>', boundary);
            if (openingEnd == std::wstring::npos)
            {
                return;
            }
            auto const name = XmlAttribute(
                std::wstring_view{ xml }.substr(searchFrom, openingEnd - searchFrom + 1),
                L"Name");
            auto const closingStart = xml.find(L"</Data>", openingEnd + 1);
            if (closingStart == std::wstring::npos)
            {
                return;
            }
            if (name && IsSensitiveDataName(*name, redactComputerName, redactUserNames))
            {
                xml.replace(openingEnd + 1, closingStart - openingEnd - 1, L"[redacted]");
                searchFrom = openingEnd + 1 + std::wstring_view{ L"[redacted]" }.size() + 7;
            }
            else
            {
                searchFrom = closingStart + 7;
            }
        }
    }

    [[nodiscard]] std::wstring RedactLiveEventForExport(
        std::wstring xml,
        bool const redactComputerName,
        bool const redactUserNames)
    {
        if (redactComputerName)
        {
            for (auto const element : {
                    L"Computer",
                    L"ComputerName",
                    L"MachineName",
                    L"Workstation",
                    L"WorkstationName" })
            {
                RedactXmlElement(xml, element);
            }
        }
        if (redactUserNames)
        {
            RedactXmlAttribute(xml, L"UserID");
            for (auto const element : {
                    L"User",
                    L"UserName",
                    L"TargetUserName",
                    L"SubjectUserName",
                    L"CallerUserName",
                    L"AccountName",
                    L"MemberName" })
            {
                RedactXmlElement(xml, element);
            }
        }
        RedactNamedEventData(xml, redactComputerName, redactUserNames);
        return xml;
    }
}

namespace winrt::AstralChronicle::implementation
{
    LiveViewModel::LiveViewModel()
        : m_events(winrt::single_threaded_observable_vector<winrt::hstring>())
    {
    }

    LiveViewModel::~LiveViewModel() noexcept
    {
        Shutdown();
    }

    void LiveViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventLiveService> liveService,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        Shutdown();
        m_liveService = std::move(liveService);
        m_strings = std::move(strings);
        if (!m_liveService || !m_strings)
        {
            throw std::invalid_argument("Live monitoring requires live and string services.");
        }
        m_dispatcher = dispatcher;
        auto const settings = ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load();
        m_queueLimit = winrt::to_hstring(settings.LiveQueueLimit);
        m_maxVisibleRows = settings.MaxVisibleLiveRows;
        m_groupRepeated = settings.GroupRepeatedEvents;
        m_redactComputerName = settings.EventItems.RedactComputerName;
        m_redactUserNames = settings.EventItems.RedactUserNames;
        m_heading = m_strings->GetString(L"Live.Heading");
        m_summary = m_strings->GetString(L"Live.Summary");
        m_stateText = m_strings->GetString(L"Live.State.Stopped");
        m_statusText = m_strings->GetString(L"Live.Ready.Text");
        m_statusDetails = m_strings->GetString(L"Live.ReadyDetails.Text");
        m_timer = dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::milliseconds{ 500 });
        auto const weakThis = get_weak();
        m_timerTickToken = m_timer.Tick([weakThis](auto const&, auto const&)
        {
            if (auto const strongThis = weakThis.get())
            {
                strongThis->OnTimerTick();
            }
        });
        m_timerTickSubscribed = true;
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
        RaiseMetricProperties();
    }

    void LiveViewModel::OnTimerTick()
    {
        if (!m_liveService) return;
        try
        {
            auto const batch = m_liveService->TakeBatch(m_isPaused ? 0u : 250u);
            ApplyBatch(batch);
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

    void LiveViewModel::ApplyBatch(::AstralChronicle::services::LiveBatch const& batch)
    {
        m_allEvents.insert(m_allEvents.end(), batch.Events.begin(), batch.Events.end());
        while (m_allEvents.size() > m_maxVisibleRows) m_allEvents.erase(m_allEvents.begin());
        RebuildEventView();

        m_droppedCount = batch.DroppedCount;
        if (m_isRecording)
        {
            auto const incomingCount = batch.Events.size();
            if (incomingCount >= MaximumRecordedEvents)
            {
                m_recordedEvents.assign(
                    batch.Events.end() - static_cast<std::ptrdiff_t>(MaximumRecordedEvents),
                    batch.Events.end());
            }
            else if (incomingCount != 0)
            {
                auto const required = m_recordedEvents.size() + incomingCount;
                if (required > MaximumRecordedEvents)
                {
                    auto const removeCount = required - MaximumRecordedEvents;
                    m_recordedEvents.erase(
                        m_recordedEvents.begin(),
                        m_recordedEvents.begin() + static_cast<std::ptrdiff_t>(removeCount));
                }
                m_recordedEvents.insert(m_recordedEvents.end(), batch.Events.begin(), batch.Events.end());
            }
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
        RaiseMetricProperties();
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
        while (values.Size() > m_maxVisibleRows) values.RemoveAt(0);
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
            if (!dispatcher || !strings) co_return;
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
                for (auto& event : events)
                {
                    content += RedactLiveEventForExport(
                        std::move(event),
                        redactComputerName,
                        redactUserNames);
                    content += L"\r\n";
                }
                co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, content);
                path = file.Path();
            }
            catch (winrt::hresult_error const& error)
            {
                errorCode = static_cast<std::uint32_t>(error.code().value);
            }
            catch (...)
            {
                errorCode = static_cast<std::uint32_t>(E_FAIL);
            }

            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || lifetimeVersion != strongThis->m_lifetimeVersion || !strongThis->m_liveService || !strings)
            {
                co_return;
            }
            if (errorCode == 0)
            {
                strongThis->SetStatus(
                    strings->GetString(L"Live.ExportCompleted.Text"),
                    FormatResource(strings->GetString(L"Live.ExportCompletedDetails.Text"), { path }),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            }
            else
            {
                strongThis->SetStatus(
                    strings->GetString(L"Live.ExportFailed.Text"),
                    FormatResource(strings->GetString(L"Live.ErrorDetails.Text"), { winrt::to_hstring(errorCode) }),
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
        try
        {
            if (m_timer)
            {
                m_timer.Stop();
                if (m_timerTickSubscribed)
                {
                    m_timer.Tick(m_timerTickToken);
                }
            }
        }
        catch (...)
        {
        }
        m_timerTickSubscribed = false;
        m_timer = nullptr;
        auto const liveService = std::move(m_liveService);
        if (liveService)
        {
            liveService->Stop();
        }
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
    winrt::event_token LiveViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_propertyChanged.add(handler); }
    void LiveViewModel::PropertyChanged(winrt::event_token const& token) noexcept { m_propertyChanged.remove(token); }
    void LiveViewModel::RaisePropertyChanged(winrt::hstring const& propertyName) { m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName }); }
}
