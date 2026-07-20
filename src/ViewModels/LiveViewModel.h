// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "LiveViewModel.g.h"
#include "Services/IEventLiveService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstdint>
#include <vector>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct LiveViewModel : LiveViewModelT<LiveViewModel>
    {
        LiveViewModel();

        void Initialize(
            ::AstralChronicle::services::IEventLiveService& liveService,
            ::AstralChronicle::design::IStringResourceService const& strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] winrt::hstring StateText() const;
        [[nodiscard]] winrt::hstring Channel() const;
        void Channel(winrt::hstring const& value);
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
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> Events() const;
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

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void OnTimerTick();
        void ApplyBatch(::AstralChronicle::services::LiveBatch const& batch);
        void SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();
        winrt::fire_and_forget ExportAsync(std::vector<std::wstring> events);

        ::AstralChronicle::services::IEventLiveService* m_liveService{};
        ::AstralChronicle::design::IStringResourceService const* m_strings{};
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
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
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_events{ nullptr };
        std::vector<std::wstring> m_recordedEvents;
        bool m_autoScroll{ true };
        bool m_groupRepeated{};
        bool m_showCritical{ true };
        bool m_showErrors{ true };
        bool m_showWarnings{ true };
        bool m_showInformation{ true };
        bool m_isRecording{};
        bool m_hasStatusMessage{ true };
        bool m_isRunning{};
        bool m_isPaused{};
        std::uint32_t m_droppedCount{};
        std::uint64_t m_totalReceived{};
        std::uint32_t m_criticalCount{};
        std::uint32_t m_errorCount{};
        std::uint32_t m_warningCount{};
        std::uint32_t m_queueDepth{};
        std::uint32_t m_bookmarkCount{};
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
