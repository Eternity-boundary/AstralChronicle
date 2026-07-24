// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "RemoteViewModel.g.h"
#include "EventLogItemViewModel.h"
#include "Services/IRemoteEventService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace AstralChronicle::design { struct IStringResourceService; }

namespace winrt::AstralChronicle::implementation
{
    struct RemoteViewModel : RemoteViewModelT<RemoteViewModel>
    {
        RemoteViewModel() = default;
        ~RemoteViewModel() noexcept;
        void Initialize(
            std::shared_ptr<::AstralChronicle::services::IRemoteEventService> service,
            std::shared_ptr<::AstralChronicle::services::IEventQueryService> localQuery,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);
        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] winrt::hstring Host() const; void Host(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring Domain() const; void Domain(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring User() const; void User(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring Password() const; void Password(winrt::hstring const& value);
        [[nodiscard]] bool IsConnected() const noexcept;
        [[nodiscard]] winrt::hstring ConnectionState() const;
        [[nodiscard]] winrt::hstring SecurityNotice() const;
        [[nodiscard]] winrt::hstring QueryChannel() const; void QueryChannel(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring QueryText() const; void QueryText(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring QueryName() const; void QueryName(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring QueryStatus() const;
        [[nodiscard]] winrt::hstring CompareStatus() const;
        [[nodiscard]] bool IsRemoteLive() const noexcept;
        [[nodiscard]] winrt::hstring LiveStatus() const;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> LiveEvents() const;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> QueryResults() const;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> SavedQueries() const;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> SavedConnections() const;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> Channels() const;
        void Connect();
        void Disconnect();
        void SaveConnection();
        void RemoveConnection();
        void TestConnection();
        void RefreshChannels();
        void SelectSavedConnection(std::int32_t index);
        void RunQuery();
        void SaveQuery(); void RemoveQuery(); void SelectSavedQuery(std::int32_t index); void CompareQuery(); void StartRemoteLive(); void StopRemoteLive();
        void Shutdown() noexcept;
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;
    private:
        struct OperationState final
        {
            std::mutex ConnectionMutex;
            std::atomic_uint64_t ConnectionVersion{};
            std::atomic_bool ShuttingDown{};
        };

        void RaisePropertyChanged(winrt::hstring const& name);
        void SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
        winrt::fire_and_forget RefreshChannelsAsync(
            std::uint64_t connectionVersion,
            std::shared_ptr<OperationState> state);
        winrt::fire_and_forget ConnectAsync(
            std::uint64_t requestVersion,
            std::shared_ptr<OperationState> state,
            std::wstring host,
            std::wstring domain,
            std::wstring user,
            std::wstring password);
        winrt::fire_and_forget DisconnectAsync(
            std::uint64_t requestVersion,
            std::shared_ptr<OperationState> state);
        winrt::fire_and_forget TestConnectionAsync(
            std::uint64_t requestVersion,
            std::shared_ptr<OperationState> state,
            std::wstring host,
            std::wstring domain,
            std::wstring user,
            std::wstring password);
        winrt::fire_and_forget QueryAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::uint64_t connectionVersion,
            std::shared_ptr<OperationState> state,
            std::wstring channel,
            std::wstring query,
            std::uint32_t maximumRecords);
        winrt::fire_and_forget CompareAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::uint64_t connectionVersion,
            std::shared_ptr<OperationState> state,
            std::wstring channel,
            std::wstring query,
            std::uint32_t maximumRecords);
        winrt::fire_and_forget LiveAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::uint64_t connectionVersion,
            std::shared_ptr<OperationState> state,
            std::wstring channel,
            std::wstring query);
        void ApplyQuery(::AstralChronicle::services::EventQueryResult const& result);
        void LoadConnections();
        [[nodiscard]] bool LoadConnectionModels();
        [[nodiscard]] bool PersistConnections() const;
        void RebuildConnectionView();
        void LoadSavedQueries();
        [[nodiscard]] bool LoadSavedQueryModels();
        [[nodiscard]] bool PersistSavedQueries() const;
        void RebuildSavedQueryView();
        std::shared_ptr<::AstralChronicle::services::IRemoteEventService> m_service;
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> m_localQuery;
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        winrt::hstring m_heading, m_summary, m_statusText, m_statusDetails, m_host, m_domain, m_user, m_password, m_connectionState, m_securityNotice, m_queryChannel{ L"System" }, m_queryText{ L"*" }, m_queryName, m_queryStatus, m_compareStatus, m_liveStatus;
        struct ConnectionProfile final
        {
            std::wstring Host;
            std::wstring Domain;
            std::wstring User;
        };
        struct QueryProfile final
        {
            std::wstring Name;
            std::wstring Channel;
            std::wstring Query;
        };
        std::vector<ConnectionProfile> m_connections;
        std::vector<QueryProfile> m_savedQueryModels;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_savedConnections{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_channels{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_savedQueries{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> m_queryResults{ nullptr };
        ::AstralChronicle::services::QueryCancellation m_queryCancellation;
        ::AstralChronicle::services::QueryCancellation m_compareCancellation;
        ::AstralChronicle::services::QueryCancellation m_liveCancellation;
        ::AstralChronicle::viewmodels::EventItemSettings m_eventItemSettings;
        std::shared_ptr<OperationState> m_operationState;
        std::uint32_t m_queryBatchSize{ 200 };
        std::uint64_t m_queryVersion{};
        std::uint64_t m_compareVersion{};
        std::uint64_t m_liveVersion{};
        std::uint64_t m_testVersion{};
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_liveEvents{ nullptr };
        bool m_connected{};
        bool m_remoteLive{};
        bool m_hasStatusMessage{ true };
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{ Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct RemoteViewModel : RemoteViewModelT<RemoteViewModel, implementation::RemoteViewModel> { };
}
