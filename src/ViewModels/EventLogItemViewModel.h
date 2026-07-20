// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "EventLogItemViewModel.g.h"
#include "Models/EventRecordSummary.h"

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct EventLogItemViewModel : EventLogItemViewModelT<EventLogItemViewModel>
    {
        EventLogItemViewModel();

        void Initialize(
            ::AstralChronicle::models::EventRecordSummary const& summary,
            ::AstralChronicle::design::IStringResourceService const& strings);

        [[nodiscard]] winrt::hstring TimeCreated() const;
        [[nodiscard]] winrt::hstring Level() const;
        [[nodiscard]] winrt::hstring Provider() const;
        [[nodiscard]] winrt::hstring EventId() const;
        [[nodiscard]] winrt::hstring Version() const;
        [[nodiscard]] winrt::hstring TaskCategory() const;
        [[nodiscard]] winrt::hstring Opcode() const;
        [[nodiscard]] winrt::hstring Keywords() const;
        [[nodiscard]] winrt::hstring Channel() const;
        [[nodiscard]] winrt::hstring User() const;
        [[nodiscard]] winrt::hstring Computer() const;
        [[nodiscard]] winrt::hstring ShortDescription() const;
        [[nodiscard]] winrt::hstring RecordId() const;
        [[nodiscard]] winrt::hstring ProcessId() const;
        [[nodiscard]] winrt::hstring ThreadId() const;
        [[nodiscard]] winrt::hstring ActivityId() const;
        [[nodiscard]] winrt::hstring RelatedActivityId() const;
        [[nodiscard]] bool IsBookmarked() const noexcept;
        void IsBookmarked(bool value);
        [[nodiscard]] std::int64_t SortTimestamp() const noexcept;
        [[nodiscard]] std::uint64_t SortRecordId() const noexcept;

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::hstring m_timeCreated;
        winrt::hstring m_level;
        winrt::hstring m_provider;
        winrt::hstring m_eventId;
        winrt::hstring m_version;
        winrt::hstring m_taskCategory;
        winrt::hstring m_opcode;
        winrt::hstring m_keywords;
        winrt::hstring m_channel;
        winrt::hstring m_user;
        winrt::hstring m_computer;
        winrt::hstring m_shortDescription;
        winrt::hstring m_recordId;
        winrt::hstring m_processId;
        winrt::hstring m_threadId;
        winrt::hstring m_activityId;
        winrt::hstring m_relatedActivityId;
        bool m_isBookmarked{};
        std::int64_t m_sortTimestamp{};
        std::uint64_t m_sortRecordId{};
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct EventLogItemViewModel : EventLogItemViewModelT<EventLogItemViewModel, implementation::EventLogItemViewModel>
    {
    };
}
