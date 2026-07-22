// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "RemoteViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "RemoteViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>

namespace
{
    [[nodiscard]] winrt::hstring FormatResource(winrt::hstring format, winrt::hstring value)
    {
        auto text = std::wstring{ format.c_str() };
        auto const marker = text.find(L"{0}");
        if (marker != std::wstring::npos) text.replace(marker, 3, value.c_str());
        return winrt::hstring{ text };
    }
}

namespace winrt::AstralChronicle::implementation
{
    void RemoteViewModel::Initialize(
        ::AstralChronicle::services::IRemoteEventService& service,
        ::AstralChronicle::services::IEventQueryService const& localQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_service = &service; m_localQuery = &localQuery; m_strings = &strings;
        m_dispatcher = dispatcher;
        m_heading = strings.GetString(L"Remote.Heading"); m_summary = strings.GetString(L"Remote.Summary");
        m_connectionState = strings.GetString(L"Remote.State.Disconnected"); m_securityNotice = strings.GetString(L"Remote.SecurityNotice.Text");
        m_statusText = strings.GetString(L"Remote.Ready.Text"); m_statusDetails = strings.GetString(L"Remote.ReadyDetails.Text");
        m_savedConnections = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_channels = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_queryResults = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_queryStatus = strings.GetString(L"Remote.QueryEmpty.Text");
        m_compareStatus = strings.GetString(L"Remote.CompareReady.Text");
        m_liveEvents = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_liveStatus = strings.GetString(L"Remote.Live.Stopped.Text");
        LoadConnections();
        LoadSavedQueries();
    }
    void RemoteViewModel::Connect()
    {
        if (!m_service || m_host.empty()) { SetStatus(m_strings->GetString(L"Remote.HostRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning); return; }
        auto const requestVersion = ++m_connectionVersion;
        m_connectionState = m_strings->GetString(L"Remote.State.Connecting");
        SetStatus(m_strings->GetString(L"Remote.Connecting.Text"), m_strings->GetString(L"Remote.ConnectingDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        RaisePropertyChanged(L"ConnectionState");
        ConnectAsync(requestVersion, std::wstring{ m_host.c_str() }, std::wstring{ m_domain.c_str() }, std::wstring{ m_user.c_str() }, std::wstring{ m_password.c_str() });
    }
    void RemoteViewModel::Disconnect()
    {
        ++m_connectionVersion;
        ++m_queryVersion;
        StopRemoteLive();
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        if (m_service) m_service->Disconnect(); m_connected = false; m_connectionState = m_strings->GetString(L"Remote.State.Disconnected");
        m_channels = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_queryResults = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_liveEvents = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_queryStatus = m_strings->GetString(L"Remote.QueryEmpty.Text");
        m_compareStatus = m_strings->GetString(L"Remote.CompareReady.Text");
        RaisePropertyChanged(L"IsConnected"); RaisePropertyChanged(L"ConnectionState");
        RaisePropertyChanged(L"Channels");
        RaisePropertyChanged(L"QueryResults");
        RaisePropertyChanged(L"LiveEvents");
        RaisePropertyChanged(L"QueryStatus");
        RaisePropertyChanged(L"CompareStatus");
    }
    void RemoteViewModel::SaveConnection()
    {
        if (m_host.empty())
        {
            SetStatus(m_strings->GetString(L"Remote.HostRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        ConnectionProfile profile{ std::wstring{ m_host.c_str() }, std::wstring{ m_domain.c_str() }, std::wstring{ m_user.c_str() } };
        auto const existing = std::find_if(m_connections.begin(), m_connections.end(), [&profile](auto const& item)
        {
            return item.Host == profile.Host && item.Domain == profile.Domain && item.User == profile.User;
        });
        if (existing == m_connections.end()) m_connections.emplace_back(std::move(profile));
        PersistConnections();
        RebuildConnectionView();
        SetStatus(m_strings->GetString(L"Remote.ConnectionSaved.Text"), m_strings->GetString(L"Remote.ConnectionSavedDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
    }
    void RemoteViewModel::RemoveConnection()
    {
        auto const host = std::wstring{ m_host.c_str() };
        m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(), [&host](auto const& item)
        {
            return item.Host == host;
        }), m_connections.end());
        PersistConnections();
        RebuildConnectionView();
        SetStatus(m_strings->GetString(L"Remote.ConnectionRemoved.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
    }
    void RemoteViewModel::TestConnection()
    {
        if (!m_service || m_host.empty())
        {
            SetStatus(m_strings->GetString(L"Remote.HostRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        auto const requestVersion = ++m_connectionVersion;
        SetStatus(m_strings->GetString(L"Remote.Testing.Text"), m_strings->GetString(L"Remote.ConnectingDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        TestConnectionAsync(requestVersion, std::wstring{ m_host.c_str() }, std::wstring{ m_domain.c_str() }, std::wstring{ m_user.c_str() }, std::wstring{ m_password.c_str() });
    }

    winrt::fire_and_forget RemoteViewModel::ConnectAsync(
        std::uint64_t const requestVersion, std::wstring host, std::wstring domain, std::wstring user, std::wstring password)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        auto const success = m_service->Connect(host, domain, user, password);
        auto const error = m_service->LastError();
        co_await wil::resume_foreground(dispatcher);
        if (requestVersion != m_connectionVersion) co_return;
        m_connected = success;
        if (success)
        {
            m_connectionState = m_strings->GetString(L"Remote.State.Connected");
            SetStatus(m_strings->GetString(L"Remote.Connected.Text"), m_strings->GetString(L"Remote.ConnectedDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        }
        else
        {
            m_connectionState = m_strings->GetString(L"Remote.State.Disconnected");
            auto detailsKey = L"Remote.ErrorDetails.Text";
            if (error == ERROR_ACCESS_DENIED) detailsKey = L"Remote.AccessDeniedDetails.Text";
            else if (error == ERROR_LOGON_FAILURE) detailsKey = L"Remote.AuthenticationDetails.Text";
            else if (error == RPC_S_SERVER_UNAVAILABLE) detailsKey = L"Remote.FirewallDetails.Text";
            SetStatus(m_strings->GetString(L"Remote.ConnectFailed.Text"), FormatResource(m_strings->GetString(detailsKey), winrt::to_hstring(error)), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
        RaisePropertyChanged(L"IsConnected"); RaisePropertyChanged(L"ConnectionState");
        if (m_connected) RefreshChannels();
    }

    winrt::fire_and_forget RemoteViewModel::TestConnectionAsync(
        std::uint64_t const requestVersion, std::wstring host, std::wstring domain, std::wstring user, std::wstring password)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        auto const success = m_service->Connect(host, domain, user, password);
        auto const error = m_service->LastError();
        m_service->Disconnect();
        co_await wil::resume_foreground(dispatcher);
        if (requestVersion != m_connectionVersion) co_return;
        if (success) SetStatus(m_strings->GetString(L"Remote.TestSucceeded.Text"), m_strings->GetString(L"Remote.TestSucceededDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        else SetStatus(m_strings->GetString(L"Remote.TestFailed.Text"), FormatResource(m_strings->GetString(L"Remote.ErrorDetails.Text"), winrt::to_hstring(error)), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
    }
    void RemoteViewModel::RefreshChannels()
    {
        if (!m_connected || !m_dispatcher) return;
        RefreshChannelsAsync();
    }

    void RemoteViewModel::RunQuery()
    {
        if (!m_connected || !m_service)
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNotConnected.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        m_queryCancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_queryVersion;
        m_queryStatus = m_strings->GetString(L"Remote.QueryLoading.Text");
        RaisePropertyChanged(L"QueryStatus");
        QueryAsync(requestVersion, m_queryCancellation, std::wstring{ m_queryChannel.c_str() }, std::wstring{ m_queryText.c_str() });
    }

    void RemoteViewModel::SaveQuery()
    {
        if (m_queryName.empty() || m_queryChannel.empty())
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNameRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        auto existing = std::find_if(m_savedQueryModels.begin(), m_savedQueryModels.end(), [this](auto const& item) { return item.Name == m_queryName.c_str(); });
        QueryProfile profile{ std::wstring{ m_queryName.c_str() }, std::wstring{ m_queryChannel.c_str() }, std::wstring{ m_queryText.c_str() } };
        if (existing == m_savedQueryModels.end()) m_savedQueryModels.emplace_back(std::move(profile));
        else *existing = std::move(profile);
        PersistSavedQueries();
        RebuildSavedQueryView();
        SetStatus(m_strings->GetString(L"Remote.QuerySaved.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
    }

    void RemoteViewModel::RemoveQuery()
    {
        auto const name = std::wstring{ m_queryName.c_str() };
        m_savedQueryModels.erase(std::remove_if(m_savedQueryModels.begin(), m_savedQueryModels.end(), [&name](auto const& item) { return item.Name == name; }), m_savedQueryModels.end());
        PersistSavedQueries();
        RebuildSavedQueryView();
        SetStatus(m_strings->GetString(L"Remote.QueryRemoved.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
    }

    void RemoteViewModel::SelectSavedQuery(std::int32_t const index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_savedQueryModels.size()) return;
        auto const& profile = m_savedQueryModels[static_cast<std::size_t>(index)];
        m_queryName = profile.Name; m_queryChannel = profile.Channel; m_queryText = profile.Query;
        RaisePropertyChanged(L"QueryName"); RaisePropertyChanged(L"QueryChannel"); RaisePropertyChanged(L"QueryText");
    }

    void RemoteViewModel::CompareQuery()
    {
        if (!m_connected || !m_service || !m_localQuery)
        {
            m_compareStatus = m_strings->GetString(L"Remote.QueryNotConnected.Text");
            RaisePropertyChanged(L"CompareStatus");
            return;
        }
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        m_queryCancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_queryVersion;
        m_compareStatus = m_strings->GetString(L"Remote.CompareLoading.Text");
        RaisePropertyChanged(L"CompareStatus");
        CompareAsync(requestVersion, m_queryCancellation, std::wstring{ m_queryChannel.c_str() }, std::wstring{ m_queryText.c_str() });
    }

    void RemoteViewModel::StartRemoteLive()
    {
        if (!m_connected || !m_service)
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNotConnected.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        if (m_remoteLive) return;
        m_remoteLive = true;
        auto const requestVersion = ++m_liveVersion;
        m_liveStatus = m_strings->GetString(L"Remote.Live.Starting.Text");
        RaisePropertyChanged(L"IsRemoteLive"); RaisePropertyChanged(L"LiveStatus");
        LiveAsync(requestVersion, std::wstring{ m_queryChannel.c_str() }, std::wstring{ m_queryText.c_str() });
    }

    void RemoteViewModel::StopRemoteLive()
    {
        ++m_liveVersion;
        m_remoteLive = false;
        if (m_service) m_service->StopLive();
        m_liveStatus = m_strings ? m_strings->GetString(L"Remote.Live.Stopped.Text") : winrt::hstring{};
        RaisePropertyChanged(L"IsRemoteLive"); RaisePropertyChanged(L"LiveStatus");
    }

    winrt::fire_and_forget RemoteViewModel::LiveAsync(std::uint64_t const requestVersion, std::wstring channel, std::wstring query)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        if (!m_service->StartLive(channel, query, 500))
        {
            auto const error = m_service->LastError();
            co_await wil::resume_foreground(dispatcher);
            if (requestVersion != m_liveVersion) co_return;
            m_remoteLive = false;
            m_liveStatus = FormatResource(m_strings->GetString(L"Remote.Live.Failed.Text"), winrt::to_hstring(error));
            RaisePropertyChanged(L"IsRemoteLive"); RaisePropertyChanged(L"LiveStatus");
            co_return;
        }
        while (requestVersion == m_liveVersion && m_remoteLive)
        {
            co_await winrt::resume_background();
            auto const batch = m_service->TakeLiveBatch(64);
            co_await wil::resume_foreground(dispatcher);
            if (requestVersion != m_liveVersion || !m_remoteLive) co_return;
            auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
            if (m_liveEvents)
            {
                for (auto const& item : m_liveEvents) values.Append(item);
            }
            for (auto const& item : batch.Events) values.Append(winrt::hstring{ item });
            while (values.Size() > 200) values.RemoveAt(0);
            m_liveEvents = values;
            if (batch.State == ::AstralChronicle::services::LiveState::Error)
            {
                m_liveStatus = FormatResource(m_strings->GetString(L"Remote.Live.Failed.Text"), winrt::to_hstring(batch.ErrorCode));
            }
            else if (batch.State == ::AstralChronicle::services::LiveState::EventsLost)
            {
                m_liveStatus = FormatResource(m_strings->GetString(L"Remote.Live.Lost.Text"), winrt::to_hstring(batch.DroppedCount));
            }
            else
            {
                m_liveStatus = FormatResource(m_strings->GetString(L"Remote.Live.Running.Text"), winrt::to_hstring(batch.QueueDepth));
            }
            RaisePropertyChanged(L"LiveEvents"); RaisePropertyChanged(L"LiveStatus");
            co_await winrt::resume_after(std::chrono::milliseconds{ 500 });
        }
    }

    winrt::fire_and_forget RemoteViewModel::QueryAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring channel,
        std::wstring query)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        auto const result = m_service->QueryPage(channel, query, 200, true, cancellation);
        co_await wil::resume_foreground(dispatcher);
        if (requestVersion != m_queryVersion || cancellation != m_queryCancellation) co_return;
        ApplyQuery(result);
    }

    winrt::fire_and_forget RemoteViewModel::CompareAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring channel,
        std::wstring query)
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        auto const remote = m_service->QueryPage(channel, query, 200, true, cancellation);
        auto const local = m_localQuery->QueryPageWithQuery(channel, query, 200, true, cancellation);
        co_await wil::resume_foreground(dispatcher);
        if (requestVersion != m_queryVersion || cancellation != m_queryCancellation) co_return;
        if (remote.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded || remote.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents)
        {
            auto const counts = std::wstring{ L"local=" } + std::to_wstring(local.Events.size()) + L", remote=" + std::to_wstring(remote.Events.size());
            m_compareStatus = FormatResource(m_strings->GetString(L"Remote.CompareReady.Text"), winrt::hstring{ counts });
        }
        else
        {
            m_compareStatus = FormatResource(m_strings->GetString(L"Remote.QueryFailed.Text"), winrt::to_hstring(remote.ErrorCode));
        }
        RaisePropertyChanged(L"CompareStatus");
    }

    void RemoteViewModel::ApplyQuery(::AstralChronicle::services::EventQueryResult const& result)
    {
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : result.Events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(event, *m_strings);
            values.Append(item);
        }
        m_queryResults = values;
        if (result.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded)
        {
            m_queryStatus = FormatResource(m_strings->GetString(L"Remote.QueryReady.Text"), winrt::to_hstring(result.Events.size()));
        }
        else if (result.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents)
        {
            m_queryStatus = m_strings->GetString(L"Remote.QueryEmpty.Text");
        }
        else
        {
            m_queryStatus = FormatResource(m_strings->GetString(L"Remote.QueryFailed.Text"), winrt::to_hstring(result.ErrorCode));
        }
        RaisePropertyChanged(L"QueryResults");
        RaisePropertyChanged(L"QueryStatus");
    }
    winrt::fire_and_forget RemoteViewModel::RefreshChannelsAsync()
    {
        auto lifetime = get_strong();
        auto dispatcher = m_dispatcher;
        co_await winrt::resume_background();
        auto const descriptors = m_service->EnumerateChannels();
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& descriptor : descriptors) values.Append(winrt::hstring{ descriptor.Path });
        co_await wil::resume_foreground(dispatcher);
        m_channels = values;
        RaisePropertyChanged(L"Channels");
    }
    void RemoteViewModel::SelectSavedConnection(std::int32_t const index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_connections.size()) return;
        auto const& profile = m_connections[static_cast<std::size_t>(index)];
        m_host = profile.Host; m_domain = profile.Domain; m_user = profile.User; m_password.clear();
        RaisePropertyChanged(L"Host"); RaisePropertyChanged(L"Domain"); RaisePropertyChanged(L"User"); RaisePropertyChanged(L"Password");
    }
    void RemoteViewModel::SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity const severity)
    {
        m_statusText = std::move(title); m_statusDetails = std::move(details); m_statusSeverity = severity; m_hasStatusMessage = true;
        RaisePropertyChanged(L"StatusText"); RaisePropertyChanged(L"StatusDetails"); RaisePropertyChanged(L"StatusSeverity"); RaisePropertyChanged(L"HasStatusMessage");
    }
    winrt::hstring RemoteViewModel::Heading() const { return m_heading; } winrt::hstring RemoteViewModel::Summary() const { return m_summary; }
    winrt::hstring RemoteViewModel::StatusText() const { return m_statusText; } winrt::hstring RemoteViewModel::StatusDetails() const { return m_statusDetails; }
    winrt::hstring RemoteViewModel::Host() const { return m_host; } void RemoteViewModel::Host(winrt::hstring const& value) { m_host = value; RaisePropertyChanged(L"Host"); }
    winrt::hstring RemoteViewModel::Domain() const { return m_domain; } void RemoteViewModel::Domain(winrt::hstring const& value) { m_domain = value; RaisePropertyChanged(L"Domain"); }
    winrt::hstring RemoteViewModel::User() const { return m_user; } void RemoteViewModel::User(winrt::hstring const& value) { m_user = value; RaisePropertyChanged(L"User"); }
    winrt::hstring RemoteViewModel::Password() const { return m_password; } void RemoteViewModel::Password(winrt::hstring const& value) { m_password = value; RaisePropertyChanged(L"Password"); }
    bool RemoteViewModel::IsConnected() const noexcept { return m_connected; } winrt::hstring RemoteViewModel::ConnectionState() const { return m_connectionState; } winrt::hstring RemoteViewModel::SecurityNotice() const { return m_securityNotice; }
    winrt::hstring RemoteViewModel::QueryChannel() const { return m_queryChannel; } void RemoteViewModel::QueryChannel(winrt::hstring const& value) { m_queryChannel = value; RaisePropertyChanged(L"QueryChannel"); }
    winrt::hstring RemoteViewModel::QueryText() const { return m_queryText; } void RemoteViewModel::QueryText(winrt::hstring const& value) { m_queryText = value; RaisePropertyChanged(L"QueryText"); }
    winrt::hstring RemoteViewModel::QueryName() const { return m_queryName; } void RemoteViewModel::QueryName(winrt::hstring const& value) { m_queryName = value; RaisePropertyChanged(L"QueryName"); }
    winrt::hstring RemoteViewModel::QueryStatus() const { return m_queryStatus; }
    winrt::hstring RemoteViewModel::CompareStatus() const { return m_compareStatus; }
    bool RemoteViewModel::IsRemoteLive() const noexcept { return m_remoteLive; }
    winrt::hstring RemoteViewModel::LiveStatus() const { return m_liveStatus; }
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> RemoteViewModel::LiveEvents() const { return m_liveEvents; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::EventLogItemViewModel> RemoteViewModel::QueryResults() const { return m_queryResults; }
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> RemoteViewModel::SavedQueries() const { return m_savedQueries; }
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> RemoteViewModel::SavedConnections() const { return m_savedConnections; }
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> RemoteViewModel::Channels() const { return m_channels; }
    Microsoft::UI::Xaml::Controls::InfoBarSeverity RemoteViewModel::StatusSeverity() const noexcept { return m_statusSeverity; } bool RemoteViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    winrt::event_token RemoteViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_propertyChanged.add(handler); }
    void RemoteViewModel::PropertyChanged(winrt::event_token const& token) noexcept { m_propertyChanged.remove(token); }
    void RemoteViewModel::RaisePropertyChanged(winrt::hstring const& name) { m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ name }); }

    void RemoteViewModel::LoadConnections()
    {
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            if (!values.HasKey(L"Remote.Connections")) return;
            auto const stored = winrt::unbox_value<winrt::hstring>(values.Lookup(L"Remote.Connections"));
            std::wstring text{ stored.c_str(), stored.size() };
            std::size_t start{};
            while (start < text.size())
            {
                auto const end = text.find(L'\n', start);
                auto const line = text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
                auto const first = line.find(L'\t');
                auto const second = first == std::wstring::npos ? std::wstring::npos : line.find(L'\t', first + 1);
                if (first != std::wstring::npos && second != std::wstring::npos)
                {
                    m_connections.push_back({ line.substr(0, first), line.substr(first + 1, second - first - 1), line.substr(second + 1) });
                }
                if (end == std::wstring::npos) break;
                start = end + 1;
            }
        }
        catch (...) { }
        RebuildConnectionView();
    }
    void RemoteViewModel::PersistConnections() const
    {
        try
        {
            std::wstring text;
            for (auto const& connection : m_connections)
            {
                if (!text.empty()) text += L'\n';
                text += connection.Host + L'\t' + connection.Domain + L'\t' + connection.User;
            }
            winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(
                L"Remote.Connections", winrt::box_value(winrt::hstring{ text }));
        }
        catch (...) { }
    }
    void RemoteViewModel::RebuildConnectionView()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& connection : m_connections)
        {
            auto label = connection.Host;
            if (!connection.User.empty())
            {
                label += L" — " + connection.User;
            }
            values.Append(winrt::hstring{ label });
        }
        m_savedConnections = values;
        RaisePropertyChanged(L"SavedConnections");
    }

    void RemoteViewModel::LoadSavedQueries()
    {
        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            if (!values.HasKey(L"Remote.SavedQueries")) { RebuildSavedQueryView(); return; }
            auto const stored = winrt::unbox_value<winrt::hstring>(values.Lookup(L"Remote.SavedQueries"));
            std::wstring text{ stored.c_str(), stored.size() };
            std::size_t start{};
            while (start < text.size())
            {
                auto const end = text.find(L'\n', start);
                auto const line = text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
                auto const first = line.find(L'\t');
                auto const second = first == std::wstring::npos ? std::wstring::npos : line.find(L'\t', first + 1);
                if (first != std::wstring::npos && second != std::wstring::npos) m_savedQueryModels.push_back({ line.substr(0, first), line.substr(first + 1, second - first - 1), line.substr(second + 1) });
                if (end == std::wstring::npos) break;
                start = end + 1;
            }
        }
        catch (...) { }
        RebuildSavedQueryView();
    }

    void RemoteViewModel::PersistSavedQueries() const
    {
        try
        {
            std::wstring text;
            for (auto const& profile : m_savedQueryModels)
            {
                if (!text.empty()) text += L'\n';
                text += profile.Name + L'\t' + profile.Channel + L'\t' + profile.Query;
            }
            winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(L"Remote.SavedQueries", winrt::box_value(winrt::hstring{ text }));
        }
        catch (...) { }
    }

    void RemoteViewModel::RebuildSavedQueryView()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& profile : m_savedQueryModels) values.Append(winrt::hstring{ profile.Name + L" — " + profile.Channel });
        m_savedQueries = values;
        RaisePropertyChanged(L"SavedQueries");
    }
}
