// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "RemoteViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/LocalDataFile.h"

#include "RemoteViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::wstring_view RemoteConnectionsFileName =
        L"remote-connections.v1.tsv";
    constexpr std::wstring_view RemoteConnectionsLegacySetting =
        L"Remote.Connections";
    constexpr std::wstring_view RemoteSavedQueriesFileName =
        L"remote-saved-queries.v1.tsv";
    constexpr std::wstring_view RemoteSavedQueriesLegacySetting =
        L"Remote.SavedQueries";
    constexpr std::size_t MaximumRemoteProfilesFileBytes = 4u * 1024u * 1024u;
    constexpr std::size_t MaximumRemoteProfileCount = 4'096u;
    constexpr std::size_t MaximumHostCharacters = 255u;
    constexpr std::size_t MaximumDomainCharacters = 255u;
    constexpr std::size_t MaximumUserCharacters = 1'024u;
    constexpr std::size_t MaximumQueryNameCharacters = 256u;
    constexpr std::size_t MaximumChannelCharacters = 32u * 1'024u;
    constexpr std::size_t MaximumQueryCharacters = 1024u * 1'024u;

    class SensitiveStringGuard final
    {
    public:
        explicit SensitiveStringGuard(std::wstring& value) noexcept
            : m_value(value)
        {
        }

        ~SensitiveStringGuard()
        {
            Clear();
        }

        SensitiveStringGuard(SensitiveStringGuard const&) = delete;
        SensitiveStringGuard& operator=(SensitiveStringGuard const&) = delete;

        void Clear() noexcept
        {
            if (!m_value.empty())
            {
                ::SecureZeroMemory(
                    m_value.data(),
                    m_value.size() * sizeof(wchar_t));
                m_value.clear();
            }
        }

    private:
        std::wstring& m_value;
    };

    [[nodiscard]] winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> CreateCoreChannelOptions()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const channel : { L"System", L"Application", L"Security", L"Setup", L"ForwardedEvents" })
        {
            values.Append(channel);
        }
        return values;
    }

    [[nodiscard]] winrt::hstring FormatResource(winrt::hstring format, winrt::hstring value)
    {
        auto text = std::wstring{ format.c_str() };
        auto const marker = text.find(L"{0}");
        if (marker != std::wstring::npos) text.replace(marker, 3, value.c_str());
        return winrt::hstring{ text };
    }

    [[nodiscard]] std::wstring EscapePersistedField(std::wstring_view const value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (auto const character : value)
        {
            switch (character)
            {
            case L'\\': result += L"\\\\"; break;
            case L'\t': result += L"\\t"; break;
            case L'\r': result += L"\\r"; break;
            case L'\n': result += L"\\n"; break;
            default: result.push_back(character); break;
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<std::wstring> UnescapePersistedField(
        std::wstring_view const value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t index{}; index < value.size(); ++index)
        {
            auto const character = value[index];
            if (character != L'\\')
            {
                if (character == L'\t' || character == L'\r' ||
                    character == L'\n' || character == L'\0')
                {
                    return std::nullopt;
                }
                result.push_back(character);
                continue;
            }

            if (index + 1 >= value.size())
            {
                return std::nullopt;
            }
            auto const escaped = value[++index];
            switch (escaped)
            {
            case L'\\': result.push_back(L'\\'); break;
            case L't': result.push_back(L'\t'); break;
            case L'r': result.push_back(L'\r'); break;
            case L'n': result.push_back(L'\n'); break;
            default:
                return std::nullopt;
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<std::vector<std::wstring>> ParseV2Record(
        std::wstring_view const line,
        std::size_t const fieldCount)
    {
        constexpr std::wstring_view Prefix = L"@v2\t";
        if (!line.starts_with(Prefix) || fieldCount == 0) return std::nullopt;

        std::vector<std::wstring> fields;
        fields.reserve(fieldCount);
        auto start = Prefix.size();
        for (std::size_t index{}; index < fieldCount; ++index)
        {
            auto const separator = line.find(L'\t', start);
            if ((index + 1 == fieldCount && separator != std::wstring_view::npos) ||
                (index + 1 != fieldCount && separator == std::wstring_view::npos))
            {
                return std::nullopt;
            }
            auto const field = line.substr(
                start,
                separator == std::wstring_view::npos ? std::wstring_view::npos : separator - start);
            auto decoded = UnescapePersistedField(field);
            if (!decoded)
            {
                return std::nullopt;
            }
            fields.emplace_back(std::move(*decoded));
            if (separator == std::wstring_view::npos) break;
            start = separator + 1;
        }
        return fields.size() == fieldCount
            ? std::optional<std::vector<std::wstring>>{ std::move(fields) }
            : std::nullopt;
    }

    [[nodiscard]] bool IsValidPersistedField(
        std::wstring_view const value,
        std::size_t const maximumCharacters,
        bool const allowEmpty,
        bool const allowLineBreaks = false) noexcept
    {
        if ((!allowEmpty && value.empty()) || value.size() > maximumCharacters ||
            value.find(L'\0') != std::wstring_view::npos)
        {
            return false;
        }
        return allowLineBreaks ||
            value.find_first_of(L"\t\r\n") == std::wstring_view::npos;
    }

    template <typename Handler>
    [[nodiscard]] bool ParsePersistedRecords(
        std::wstring_view const text,
        Handler&& handler)
    {
        if (text.size() > MaximumRemoteProfilesFileBytes)
        {
            return false;
        }
        if (text.empty())
        {
            return true;
        }

        std::size_t count{};
        std::size_t start{};
        while (start < text.size())
        {
            if (++count > MaximumRemoteProfileCount)
            {
                return false;
            }
            auto const end = text.find(L'\n', start);
            auto const line = text.substr(
                start,
                end == std::wstring_view::npos
                    ? std::wstring_view::npos
                    : end - start);
            if (line.empty() || !handler(line))
            {
                return false;
            }
            if (end == std::wstring_view::npos)
            {
                return true;
            }
            start = end + 1;
        }
        return true;
    }
}

namespace winrt::AstralChronicle::implementation
{
    RemoteViewModel::~RemoteViewModel() noexcept
    {
        Shutdown();
    }

    void RemoteViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IRemoteEventService> service,
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> localQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        Shutdown();
        m_service = std::move(service);
        m_localQuery = std::move(localQuery);
        m_strings = std::move(strings);
        if (!m_service || !m_localQuery || !m_strings)
        {
            throw std::invalid_argument("Remote events require remote, local query, and string services.");
        }
        m_operationState = std::make_shared<OperationState>();
        m_dispatcher = dispatcher;
        auto const settings = ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load();
        m_eventItemSettings = settings.EventItems;
        m_queryBatchSize = settings.QueryBatchSize;
        m_heading = m_strings->GetString(L"Remote.Heading"); m_summary = m_strings->GetString(L"Remote.Summary");
        m_connectionState = m_strings->GetString(L"Remote.State.Disconnected"); m_securityNotice = m_strings->GetString(L"Remote.SecurityNotice.Text");
        m_statusText = m_strings->GetString(L"Remote.Ready.Text"); m_statusDetails = m_strings->GetString(L"Remote.ReadyDetails.Text");
        m_savedConnections = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_channels = CreateCoreChannelOptions();
        m_queryResults = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_queryStatus = m_strings->GetString(L"Remote.QueryEmpty.Text");
        m_compareStatus = m_strings->GetString(L"Remote.CompareReady.Text");
        m_liveEvents = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_liveStatus = m_strings->GetString(L"Remote.Live.Stopped.Text");
        LoadConnections();
        LoadSavedQueries();
    }
    void RemoteViewModel::Connect()
    {
        auto const state = m_operationState;
        if (!m_service || !m_strings || !state || m_host.empty()) { if (m_strings) SetStatus(m_strings->GetString(L"Remote.HostRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning); return; }
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        if (m_compareCancellation) m_compareCancellation->store(true, std::memory_order_relaxed);
        ++m_queryVersion;
        ++m_compareVersion;
        ++m_testVersion;
        StopRemoteLive();
        auto const requestVersion = state->ConnectionVersion.fetch_add(1, std::memory_order_acq_rel) + 1;
        m_connected = false;
        m_connectionState = m_strings->GetString(L"Remote.State.Connecting");
        m_channels = CreateCoreChannelOptions();
        m_queryResults = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        m_liveEvents = winrt::single_threaded_observable_vector<winrt::hstring>();
        m_queryStatus = m_strings->GetString(L"Remote.QueryEmpty.Text");
        m_compareStatus = m_strings->GetString(L"Remote.CompareReady.Text");
        SetStatus(m_strings->GetString(L"Remote.Connecting.Text"), m_strings->GetString(L"Remote.ConnectingDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        RaisePropertyChanged(L"IsConnected");
        RaisePropertyChanged(L"ConnectionState");
        RaisePropertyChanged(L"Channels");
        RaisePropertyChanged(L"QueryResults");
        RaisePropertyChanged(L"LiveEvents");
        RaisePropertyChanged(L"QueryStatus");
        RaisePropertyChanged(L"CompareStatus");
        auto password = std::wstring{ m_password.c_str() };
        m_password.clear();
        RaisePropertyChanged(L"Password");
        ConnectAsync(
            requestVersion,
            state,
            std::wstring{ m_host.c_str() },
            std::wstring{ m_domain.c_str() },
            std::wstring{ m_user.c_str() },
            std::move(password));
    }
    void RemoteViewModel::Disconnect()
    {
        auto const state = m_operationState;
        if (!state) return;
        auto const requestVersion = state->ConnectionVersion.fetch_add(1, std::memory_order_acq_rel) + 1;
        ++m_queryVersion;
        ++m_compareVersion;
        ++m_testVersion;
        StopRemoteLive();
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        if (m_compareCancellation) m_compareCancellation->store(true, std::memory_order_relaxed);
        DisconnectAsync(requestVersion, state);
        m_connected = false; m_connectionState = m_strings->GetString(L"Remote.State.Disconnected");
        m_channels = CreateCoreChannelOptions();
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
        ConnectionProfile profile{
            std::wstring{ m_host.c_str() },
            std::wstring{ m_domain.c_str() },
            std::wstring{ m_user.c_str() },
        };
        if (!IsValidPersistedField(
                profile.Host,
                MaximumHostCharacters,
                false) ||
            !IsValidPersistedField(
                profile.Domain,
                MaximumDomainCharacters,
                true) ||
            !IsValidPersistedField(
                profile.User,
                MaximumUserCharacters,
                true))
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }

        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteConnectionsFileName,
        };
        if (!transaction || !LoadConnectionModels())
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }

        auto const original = m_connections;
        auto const existing = std::find_if(m_connections.begin(), m_connections.end(), [&profile](auto const& item)
        {
            return item.Host == profile.Host && item.Domain == profile.Domain && item.User == profile.User;
        });
        if (existing == m_connections.end())
        {
            if (m_connections.size() >= MaximumRemoteProfileCount)
            {
                SetStatus(
                    m_strings->GetString(L"Remote.StorageFailed.Text"),
                    m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
                return;
            }
            m_connections.emplace_back(std::move(profile));
        }
        if (!PersistConnections())
        {
            m_connections = original;
            RebuildConnectionView();
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        RebuildConnectionView();
        SetStatus(m_strings->GetString(L"Remote.ConnectionSaved.Text"), m_strings->GetString(L"Remote.ConnectionSavedDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
    }
    void RemoteViewModel::RemoveConnection()
    {
        ConnectionProfile const profile{
            std::wstring{ m_host.c_str() },
            std::wstring{ m_domain.c_str() },
            std::wstring{ m_user.c_str() } };
        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteConnectionsFileName,
        };
        if (!transaction || !LoadConnectionModels())
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        auto const original = m_connections;
        m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(), [&profile](auto const& item)
        {
            return item.Host == profile.Host &&
                item.Domain == profile.Domain &&
                item.User == profile.User;
        }), m_connections.end());
        if (!PersistConnections())
        {
            m_connections = original;
            RebuildConnectionView();
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
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
        auto const state = m_operationState;
        if (!state) return;
        auto const requestVersion = ++m_testVersion;
        SetStatus(m_strings->GetString(L"Remote.Testing.Text"), m_strings->GetString(L"Remote.ConnectingDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        auto password = std::wstring{ m_password.c_str() };
        m_password.clear();
        RaisePropertyChanged(L"Password");
        TestConnectionAsync(
            requestVersion,
            state,
            std::wstring{ m_host.c_str() },
            std::wstring{ m_domain.c_str() },
            std::wstring{ m_user.c_str() },
            std::move(password));
    }

    winrt::fire_and_forget RemoteViewModel::ConnectAsync(
        std::uint64_t const requestVersion,
        std::shared_ptr<OperationState> state,
        std::wstring host,
        std::wstring domain,
        std::wstring user,
        std::wstring password)
    {
        auto const service = m_service;
        auto const weakThis = get_weak();
        SensitiveStringGuard passwordGuard{ password };
        try
        {
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !state || state->ShuttingDown.load(std::memory_order_acquire)) co_return;

            bool success{};
            std::uint32_t error{};
            {
                std::scoped_lock const lock{ state->ConnectionMutex };
                if (state->ShuttingDown.load(std::memory_order_acquire) ||
                    requestVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
                try
                {
                    success = service->Connect(host, domain, user, password);
                    error = service->LastError();
                }
                catch (winrt::hresult_error const& exception)
                {
                    error = static_cast<std::uint32_t>(exception.code().value);
                }
                catch (...)
                {
                    error = static_cast<std::uint32_t>(E_FAIL);
                }

                if (state->ShuttingDown.load(std::memory_order_acquire) ||
                    requestVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
            }

            passwordGuard.Clear();
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || strongThis->m_operationState != state ||
                requestVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }
            strongThis->m_connected = success;
            if (success)
            {
                strongThis->m_connectionState = strongThis->m_strings->GetString(L"Remote.State.Connected");
                strongThis->SetStatus(strongThis->m_strings->GetString(L"Remote.Connected.Text"), strongThis->m_strings->GetString(L"Remote.ConnectedDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            }
            else
            {
                strongThis->m_connectionState = strongThis->m_strings->GetString(L"Remote.State.Disconnected");
                auto detailsKey = L"Remote.ErrorDetails.Text";
                if (error == ERROR_ACCESS_DENIED) detailsKey = L"Remote.AccessDeniedDetails.Text";
                else if (error == ERROR_LOGON_FAILURE) detailsKey = L"Remote.AuthenticationDetails.Text";
                else if (error == RPC_S_SERVER_UNAVAILABLE) detailsKey = L"Remote.FirewallDetails.Text";
                strongThis->SetStatus(strongThis->m_strings->GetString(L"Remote.ConnectFailed.Text"), FormatResource(strongThis->m_strings->GetString(detailsKey), winrt::to_hstring(error)), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            }
            strongThis->RaisePropertyChanged(L"IsConnected"); strongThis->RaisePropertyChanged(L"ConnectionState");
            if (strongThis->m_connected) strongThis->RefreshChannels();
        }
        catch (...)
        {
            co_return;
        }
    }

    winrt::fire_and_forget RemoteViewModel::DisconnectAsync(
        std::uint64_t const requestVersion,
        std::shared_ptr<OperationState> state)
    {
        auto const service = m_service;
        try
        {
            co_await winrt::resume_background();
            if (!service || !state) co_return;
            std::scoped_lock const lock{ state->ConnectionMutex };
            if (!state->ShuttingDown.load(std::memory_order_acquire) &&
                requestVersion == state->ConnectionVersion.load(std::memory_order_acquire))
            {
                service->Disconnect();
            }
        }
        catch (...)
        {
            co_return;
        }
    }

    winrt::fire_and_forget RemoteViewModel::TestConnectionAsync(
        std::uint64_t const requestVersion,
        std::shared_ptr<OperationState> state,
        std::wstring host,
        std::wstring domain,
        std::wstring user,
        std::wstring password)
    {
        auto const service = m_service;
        SensitiveStringGuard passwordGuard{ password };
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !state || state->ShuttingDown.load(std::memory_order_acquire)) co_return;
            ::AstralChronicle::services::RemoteProbeResult probe;
            try
            {
                probe = service->Probe(host, domain, user, password);
            }
            catch (winrt::hresult_error const& exception)
            {
                probe.ErrorCode = static_cast<std::uint32_t>(exception.code().value);
            }
            catch (...)
            {
                probe.ErrorCode = static_cast<std::uint32_t>(E_FAIL);
            }
            passwordGuard.Clear();
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || strongThis->m_operationState != state ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                requestVersion != strongThis->m_testVersion)
            {
                co_return;
            }
            if (probe.Success) strongThis->SetStatus(strongThis->m_strings->GetString(L"Remote.TestSucceeded.Text"), strongThis->m_strings->GetString(L"Remote.TestSucceededDetails.Text"), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            else strongThis->SetStatus(strongThis->m_strings->GetString(L"Remote.TestFailed.Text"), FormatResource(strongThis->m_strings->GetString(L"Remote.ErrorDetails.Text"), winrt::to_hstring(probe.ErrorCode)), Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
        catch (...)
        {
            co_return;
        }
    }
    void RemoteViewModel::RefreshChannels()
    {
        auto const state = m_operationState;
        if (!m_connected || !m_dispatcher || !state) return;
        RefreshChannelsAsync(state->ConnectionVersion.load(std::memory_order_acquire), state);
    }

    void RemoteViewModel::RunQuery()
    {
        auto const state = m_operationState;
        if (!m_connected || !m_service || !state)
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNotConnected.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        m_queryCancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_queryVersion;
        m_queryStatus = m_strings->GetString(L"Remote.QueryLoading.Text");
        RaisePropertyChanged(L"QueryStatus");
        QueryAsync(
            requestVersion,
            m_queryCancellation,
            state->ConnectionVersion.load(std::memory_order_acquire),
            state,
            std::wstring{ m_queryChannel.c_str() },
            std::wstring{ m_queryText.c_str() },
            m_queryBatchSize);
    }

    void RemoteViewModel::SaveQuery()
    {
        if (m_queryName.empty() || m_queryChannel.empty())
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNameRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        QueryProfile profile{
            std::wstring{ m_queryName.c_str() },
            std::wstring{ m_queryChannel.c_str() },
            std::wstring{ m_queryText.c_str() },
        };
        if (!IsValidPersistedField(
                profile.Name,
                MaximumQueryNameCharacters,
                false) ||
            !IsValidPersistedField(
                profile.Channel,
                MaximumChannelCharacters,
                false) ||
            !IsValidPersistedField(
                profile.Query,
                MaximumQueryCharacters,
                true,
                true))
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }

        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteSavedQueriesFileName,
        };
        if (!transaction || !LoadSavedQueryModels())
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        auto const original = m_savedQueryModels;
        auto existing = std::find_if(
            m_savedQueryModels.begin(),
            m_savedQueryModels.end(),
            [&profile](auto const& item)
            {
                return item.Name == profile.Name;
            });
        if (existing == m_savedQueryModels.end())
        {
            if (m_savedQueryModels.size() >= MaximumRemoteProfileCount)
            {
                SetStatus(
                    m_strings->GetString(L"Remote.StorageFailed.Text"),
                    m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
                return;
            }
            m_savedQueryModels.emplace_back(std::move(profile));
        }
        else *existing = std::move(profile);
        if (!PersistSavedQueries())
        {
            m_savedQueryModels = original;
            RebuildSavedQueryView();
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        RebuildSavedQueryView();
        SetStatus(m_strings->GetString(L"Remote.QuerySaved.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
    }

    void RemoteViewModel::RemoveQuery()
    {
        auto const name = std::wstring{ m_queryName.c_str() };
        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteSavedQueriesFileName,
        };
        if (!transaction || !LoadSavedQueryModels())
        {
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        auto const original = m_savedQueryModels;
        m_savedQueryModels.erase(std::remove_if(m_savedQueryModels.begin(), m_savedQueryModels.end(), [&name](auto const& item) { return item.Name == name; }), m_savedQueryModels.end());
        if (!PersistSavedQueries())
        {
            m_savedQueryModels = original;
            RebuildSavedQueryView();
            SetStatus(
                m_strings->GetString(L"Remote.StorageFailed.Text"),
                m_strings->GetString(L"Remote.StorageFailedDetails.Text"),
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
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
        auto const state = m_operationState;
        if (!m_connected || !m_service || !m_localQuery || !state)
        {
            m_compareStatus = m_strings->GetString(L"Remote.QueryNotConnected.Text");
            RaisePropertyChanged(L"CompareStatus");
            return;
        }
        if (m_compareCancellation) m_compareCancellation->store(true, std::memory_order_relaxed);
        m_compareCancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_compareVersion;
        m_compareStatus = m_strings->GetString(L"Remote.CompareLoading.Text");
        RaisePropertyChanged(L"CompareStatus");
        CompareAsync(
            requestVersion,
            m_compareCancellation,
            state->ConnectionVersion.load(std::memory_order_acquire),
            state,
            std::wstring{ m_queryChannel.c_str() },
            std::wstring{ m_queryText.c_str() },
            m_queryBatchSize);
    }

    void RemoteViewModel::StartRemoteLive()
    {
        if (!m_connected || !m_service)
        {
            SetStatus(m_strings->GetString(L"Remote.QueryNotConnected.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            return;
        }
        if (m_remoteLive) return;
        auto const state = m_operationState;
        if (!state) return;
        if (m_liveCancellation) m_liveCancellation->store(true, std::memory_order_relaxed);
        m_liveCancellation = ::AstralChronicle::services::MakeQueryCancellation();
        m_remoteLive = true;
        auto const requestVersion = ++m_liveVersion;
        m_liveStatus = m_strings->GetString(L"Remote.Live.Starting.Text");
        RaisePropertyChanged(L"IsRemoteLive"); RaisePropertyChanged(L"LiveStatus");
        LiveAsync(
            requestVersion,
            m_liveCancellation,
            state->ConnectionVersion.load(std::memory_order_acquire),
            state,
            std::wstring{ m_queryChannel.c_str() },
            std::wstring{ m_queryText.c_str() });
    }

    void RemoteViewModel::StopRemoteLive()
    {
        ++m_liveVersion;
        if (m_liveCancellation) m_liveCancellation->store(true, std::memory_order_relaxed);
        m_remoteLive = false;
        if (m_service) m_service->StopLive();
        m_liveStatus = m_strings ? m_strings->GetString(L"Remote.Live.Stopped.Text") : winrt::hstring{};
        RaisePropertyChanged(L"IsRemoteLive"); RaisePropertyChanged(L"LiveStatus");
    }

    winrt::fire_and_forget RemoteViewModel::LiveAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::uint64_t const connectionVersion,
        std::shared_ptr<OperationState> state,
        std::wstring channel,
        std::wstring query)
    {
        auto const service = m_service;
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !state || !cancellation ||
                cancellation->load(std::memory_order_relaxed) ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }

            bool started{};
            std::uint32_t error{};
            {
                std::scoped_lock const lock{ state->ConnectionMutex };
                if (!cancellation->load(std::memory_order_relaxed) &&
                    !state->ShuttingDown.load(std::memory_order_acquire) &&
                    connectionVersion == state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    try
                    {
                        started = service->StartLive(channel, query, 500);
                        error = service->LastError();
                    }
                    catch (winrt::hresult_error const& exception)
                    {
                        error = static_cast<std::uint32_t>(exception.code().value);
                    }
                    catch (...)
                    {
                        error = static_cast<std::uint32_t>(E_FAIL);
                    }
                }
            }
            if (!started)
            {
                if (cancellation->load(std::memory_order_relaxed) ||
                    state->ShuttingDown.load(std::memory_order_acquire) ||
                    connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
                co_await wil::resume_foreground(dispatcher);
                auto const strongThis = weakThis.get();
                if (!strongThis || strongThis->m_operationState != state ||
                    requestVersion != strongThis->m_liveVersion ||
                    cancellation != strongThis->m_liveCancellation)
                {
                    co_return;
                }
                strongThis->m_remoteLive = false;
                cancellation->store(true, std::memory_order_relaxed);
                strongThis->m_liveStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.Live.Failed.Text"), winrt::to_hstring(error));
                strongThis->RaisePropertyChanged(L"IsRemoteLive"); strongThis->RaisePropertyChanged(L"LiveStatus");
                co_return;
            }

            if (cancellation->load(std::memory_order_relaxed) ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }

            while (!cancellation->load(std::memory_order_relaxed) &&
                !state->ShuttingDown.load(std::memory_order_acquire) &&
                connectionVersion == state->ConnectionVersion.load(std::memory_order_acquire))
            {
                ::AstralChronicle::services::LiveBatch batch;
                try
                {
                    batch = service->TakeLiveBatch(64);
                }
                catch (winrt::hresult_error const& exception)
                {
                    batch.State = ::AstralChronicle::services::LiveState::Error;
                    batch.ErrorCode = static_cast<std::uint32_t>(exception.code().value);
                }
                catch (...)
                {
                    batch.State = ::AstralChronicle::services::LiveState::Error;
                    batch.ErrorCode = static_cast<std::uint32_t>(E_FAIL);
                }
                co_await wil::resume_foreground(dispatcher);
                bool terminal{};
                {
                    auto const strongThis = weakThis.get();
                    if (!strongThis || strongThis->m_operationState != state ||
                        requestVersion != strongThis->m_liveVersion ||
                        cancellation != strongThis->m_liveCancellation ||
                        cancellation->load(std::memory_order_relaxed))
                    {
                        co_return;
                    }
                    auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
                    if (strongThis->m_liveEvents)
                    {
                        for (auto const& item : strongThis->m_liveEvents) values.Append(item);
                    }
                    for (auto const& item : batch.Events) values.Append(winrt::hstring{ item });
                    while (values.Size() > 200) values.RemoveAt(0);
                    strongThis->m_liveEvents = values;
                    if (batch.State == ::AstralChronicle::services::LiveState::Error)
                    {
                        strongThis->m_liveStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.Live.Failed.Text"), winrt::to_hstring(batch.ErrorCode));
                        terminal = true;
                    }
                    else if (batch.State == ::AstralChronicle::services::LiveState::Stopped ||
                        batch.State == ::AstralChronicle::services::LiveState::NotConfigured)
                    {
                        strongThis->m_liveStatus = strongThis->m_strings->GetString(L"Remote.Live.Stopped.Text");
                        terminal = true;
                    }
                    else if (batch.State == ::AstralChronicle::services::LiveState::EventsLost)
                    {
                        strongThis->m_liveStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.Live.Lost.Text"), winrt::to_hstring(batch.DroppedCount));
                    }
                    else
                    {
                        strongThis->m_liveStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.Live.Running.Text"), winrt::to_hstring(batch.QueueDepth));
                    }
                    if (terminal)
                    {
                        strongThis->m_remoteLive = false;
                        cancellation->store(true, std::memory_order_relaxed);
                        strongThis->RaisePropertyChanged(L"IsRemoteLive");
                    }
                    strongThis->RaisePropertyChanged(L"LiveEvents"); strongThis->RaisePropertyChanged(L"LiveStatus");
                }
                if (terminal)
                {
                    service->StopLive();
                    co_return;
                }
                co_await winrt::resume_after(std::chrono::milliseconds{ 500 });
                co_await winrt::resume_background();
            }
        }
        catch (...)
        {
            if (service && state && cancellation &&
                !cancellation->load(std::memory_order_relaxed) &&
                connectionVersion == state->ConnectionVersion.load(std::memory_order_acquire) &&
                !state->ShuttingDown.load(std::memory_order_acquire))
            {
                service->StopLive();
            }
            co_return;
        }
    }

    winrt::fire_and_forget RemoteViewModel::QueryAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::uint64_t const connectionVersion,
        std::shared_ptr<OperationState> state,
        std::wstring channel,
        std::wstring query,
        std::uint32_t const maximumRecords)
    {
        auto const service = m_service;
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !state || !cancellation ||
                cancellation->load(std::memory_order_relaxed) ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }
            ::AstralChronicle::services::EventQueryResult result;
            {
                std::scoped_lock const lock{ state->ConnectionMutex };
                if (cancellation->load(std::memory_order_relaxed) ||
                    state->ShuttingDown.load(std::memory_order_acquire) ||
                    connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
                result = service->QueryPage(channel, query, maximumRecords, true, cancellation);
            }
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || strongThis->m_operationState != state ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire) ||
                requestVersion != strongThis->m_queryVersion ||
                cancellation != strongThis->m_queryCancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            strongThis->ApplyQuery(result);
        }
        catch (...)
        {
            co_return;
        }
    }

    winrt::fire_and_forget RemoteViewModel::CompareAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::uint64_t const connectionVersion,
        std::shared_ptr<OperationState> state,
        std::wstring channel,
        std::wstring query,
        std::uint32_t const maximumRecords)
    {
        auto const service = m_service;
        auto const localQuery = m_localQuery;
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !localQuery || !state || !cancellation ||
                cancellation->load(std::memory_order_relaxed) ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }
            ::AstralChronicle::services::EventQueryResult remote;
            {
                std::scoped_lock const lock{ state->ConnectionMutex };
                if (cancellation->load(std::memory_order_relaxed) ||
                    state->ShuttingDown.load(std::memory_order_acquire) ||
                    connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
                remote = service->QueryPage(channel, query, maximumRecords, true, cancellation);
            }
            auto const local = localQuery->QueryPageWithQuery(channel, query, maximumRecords, true, cancellation);
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || strongThis->m_operationState != state ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire) ||
                requestVersion != strongThis->m_compareVersion ||
                cancellation != strongThis->m_compareCancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            auto const remoteSucceeded = remote.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded || remote.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents;
            auto const localSucceeded = local.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded || local.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents;
            if (remoteSucceeded && localSucceeded)
            {
                auto const counts = std::wstring{ L"local=" } + std::to_wstring(local.Events.size()) + L", remote=" + std::to_wstring(remote.Events.size());
                strongThis->m_compareStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.CompareReady.Text"), winrt::hstring{ counts });
            }
            else
            {
                auto const error = remoteSucceeded ? local.ErrorCode : remote.ErrorCode;
                strongThis->m_compareStatus = FormatResource(strongThis->m_strings->GetString(L"Remote.QueryFailed.Text"), winrt::to_hstring(error));
            }
            strongThis->RaisePropertyChanged(L"CompareStatus");
        }
        catch (...)
        {
            co_return;
        }
    }

    void RemoteViewModel::ApplyQuery(::AstralChronicle::services::EventQueryResult const& result)
    {
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::EventLogItemViewModel>();
        for (auto const& event : result.Events)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::EventLogItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::EventLogItemViewModel>(item)->Initialize(
                event,
                *m_strings,
                m_eventItemSettings);
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
    winrt::fire_and_forget RemoteViewModel::RefreshChannelsAsync(
        std::uint64_t const connectionVersion,
        std::shared_ptr<OperationState> state)
    {
        auto const service = m_service;
        try
        {
            auto const weakThis = get_weak();
            auto const dispatcher = m_dispatcher;
            co_await winrt::resume_background();
            if (!service || !state || state->ShuttingDown.load(std::memory_order_acquire)) co_return;
            std::vector<::AstralChronicle::models::EventChannelDescriptor> descriptors;
            {
                std::scoped_lock const lock{ state->ConnectionMutex };
                if (state->ShuttingDown.load(std::memory_order_acquire) ||
                    connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
                {
                    co_return;
                }
                descriptors = service->EnumerateChannels();
            }
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || strongThis->m_operationState != state ||
                state->ShuttingDown.load(std::memory_order_acquire) ||
                connectionVersion != state->ConnectionVersion.load(std::memory_order_acquire))
            {
                co_return;
            }
            auto values = CreateCoreChannelOptions();
            for (auto const& descriptor : descriptors)
            {
                bool exists{};
                for (std::uint32_t index{}; index < values.Size(); ++index)
                {
                    if (std::wstring{ values.GetAt(index).c_str() } == descriptor.Path)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists) values.Append(winrt::hstring{ descriptor.Path });
            }
            strongThis->m_channels = values;
            strongThis->RaisePropertyChanged(L"Channels");
        }
        catch (...)
        {
            co_return;
        }
    }
    void RemoteViewModel::SelectSavedConnection(std::int32_t const index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_connections.size()) return;
        auto const& profile = m_connections[static_cast<std::size_t>(index)];
        m_host = profile.Host; m_domain = profile.Domain; m_user = profile.User; m_password.clear();
        RaisePropertyChanged(L"Host"); RaisePropertyChanged(L"Domain"); RaisePropertyChanged(L"User"); RaisePropertyChanged(L"Password");
    }

    void RemoteViewModel::Shutdown() noexcept
    {
        auto const state = m_operationState;
        m_operationState.reset();
        if (state)
        {
            state->ShuttingDown.store(true, std::memory_order_release);
            state->ConnectionVersion.fetch_add(1, std::memory_order_acq_rel);
        }
        if (m_queryCancellation) m_queryCancellation->store(true, std::memory_order_relaxed);
        if (m_compareCancellation) m_compareCancellation->store(true, std::memory_order_relaxed);
        if (m_liveCancellation) m_liveCancellation->store(true, std::memory_order_relaxed);
        ++m_queryVersion;
        ++m_compareVersion;
        ++m_liveVersion;
        ++m_testVersion;

        auto const service = std::move(m_service);
        if (service)
        {
            service->StopLive();
            service->Disconnect();
        }
        m_connected = false;
        m_remoteLive = false;
        m_localQuery.reset();
        m_strings.reset();
        m_dispatcher = nullptr;
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
        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteConnectionsFileName,
        };
        if (transaction)
        {
            (void)LoadConnectionModels();
        }
        RebuildConnectionView();
    }

    bool RemoteViewModel::LoadConnectionModels()
    {
        auto parse = [](std::wstring_view const text)
            -> std::optional<std::vector<ConnectionProfile>>
        {
            std::vector<ConnectionProfile> parsed;
            auto const succeeded = ParsePersistedRecords(
                text,
                [&parsed](std::wstring_view const line)
                {
                    std::vector<std::wstring> fields;
                    if (line.starts_with(L"@v2\t"))
                    {
                        auto decoded = ParseV2Record(line, 3);
                        if (!decoded)
                        {
                            return false;
                        }
                        fields = std::move(*decoded);
                    }
                    else
                    {
                        auto const first = line.find(L'\t');
                        auto const second = first == std::wstring_view::npos
                            ? std::wstring_view::npos
                            : line.find(L'\t', first + 1);
                        if (first == std::wstring_view::npos ||
                            second == std::wstring_view::npos ||
                            line.find(L'\t', second + 1) != std::wstring_view::npos)
                        {
                            return false;
                        }
                        fields.emplace_back(line.substr(0, first));
                        fields.emplace_back(
                            line.substr(first + 1, second - first - 1));
                        fields.emplace_back(line.substr(second + 1));
                    }

                    if (!IsValidPersistedField(
                            fields[0],
                            MaximumHostCharacters,
                            false) ||
                        !IsValidPersistedField(
                            fields[1],
                            MaximumDomainCharacters,
                            true) ||
                        !IsValidPersistedField(
                            fields[2],
                            MaximumUserCharacters,
                            true))
                    {
                        return false;
                    }
                    parsed.push_back({
                        std::move(fields[0]),
                        std::move(fields[1]),
                        std::move(fields[2]),
                    });
                    return true;
                });
            return succeeded
                ? std::optional<std::vector<ConnectionProfile>>{
                    std::move(parsed),
                }
                : std::nullopt;
        };

        auto const file = ::AstralChronicle::services::details::ReadLocalUtf8Text(
            RemoteConnectionsFileName,
            MaximumRemoteProfilesFileBytes);
        if (file.Status ==
            ::AstralChronicle::services::details::LocalTextReadStatus::Succeeded)
        {
            auto parsed = parse(file.Text);
            if (!parsed)
            {
                return false;
            }
            m_connections = std::move(*parsed);
            try
            {
                winrt::Windows::Storage::ApplicationData::Current()
                    .LocalSettings()
                    .Values()
                    .Remove(winrt::hstring{ RemoteConnectionsLegacySetting });
            }
            catch (...)
            {
            }
            return true;
        }
        if (file.Status ==
            ::AstralChronicle::services::details::LocalTextReadStatus::Failed)
        {
            return false;
        }

        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const legacyKey = winrt::hstring{
                RemoteConnectionsLegacySetting,
            };
            if (!values.HasKey(legacyKey))
            {
                m_connections.clear();
                return true;
            }
            auto const stored =
                winrt::unbox_value<winrt::hstring>(values.Lookup(legacyKey));
            auto parsed = parse(
                std::wstring_view{ stored.c_str(), stored.size() });
            if (!parsed)
            {
                return false;
            }
            m_connections = std::move(*parsed);
            if (PersistConnections())
            {
                values.Remove(legacyKey);
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool RemoteViewModel::PersistConnections() const
    {
        try
        {
            std::wstring text;
            for (auto const& connection : m_connections)
            {
                if (!text.empty()) text += L'\n';
                text += L"@v2\t" + EscapePersistedField(connection.Host) + L'\t' +
                    EscapePersistedField(connection.Domain) + L'\t' +
                    EscapePersistedField(connection.User);
            }
            return ::AstralChronicle::services::details::WriteLocalUtf8TextAtomically(
                RemoteConnectionsFileName,
                text,
                MaximumRemoteProfilesFileBytes);
        }
        catch (...)
        {
            return false;
        }
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
        ::AstralChronicle::services::details::LocalDataTransactionLock const transaction{
            RemoteSavedQueriesFileName,
        };
        if (transaction)
        {
            (void)LoadSavedQueryModels();
        }
        RebuildSavedQueryView();
    }

    bool RemoteViewModel::LoadSavedQueryModels()
    {
        auto parse = [](std::wstring_view const text)
            -> std::optional<std::vector<QueryProfile>>
        {
            std::vector<QueryProfile> parsed;
            auto const succeeded = ParsePersistedRecords(
                text,
                [&parsed](std::wstring_view const line)
                {
                    std::vector<std::wstring> fields;
                    if (line.starts_with(L"@v2\t"))
                    {
                        auto decoded = ParseV2Record(line, 3);
                        if (!decoded)
                        {
                            return false;
                        }
                        fields = std::move(*decoded);
                    }
                    else
                    {
                        auto const first = line.find(L'\t');
                        auto const second = first == std::wstring_view::npos
                            ? std::wstring_view::npos
                            : line.find(L'\t', first + 1);
                        if (first == std::wstring_view::npos ||
                            second == std::wstring_view::npos)
                        {
                            return false;
                        }
                        fields.emplace_back(line.substr(0, first));
                        fields.emplace_back(
                            line.substr(first + 1, second - first - 1));
                        fields.emplace_back(line.substr(second + 1));
                    }

                    if (!IsValidPersistedField(
                            fields[0],
                            MaximumQueryNameCharacters,
                            false) ||
                        !IsValidPersistedField(
                            fields[1],
                            MaximumChannelCharacters,
                            false) ||
                        !IsValidPersistedField(
                            fields[2],
                            MaximumQueryCharacters,
                            true,
                            true))
                    {
                        return false;
                    }
                    parsed.push_back({
                        std::move(fields[0]),
                        std::move(fields[1]),
                        std::move(fields[2]),
                    });
                    return true;
                });
            return succeeded
                ? std::optional<std::vector<QueryProfile>>{
                    std::move(parsed),
                }
                : std::nullopt;
        };

        auto const file = ::AstralChronicle::services::details::ReadLocalUtf8Text(
            RemoteSavedQueriesFileName,
            MaximumRemoteProfilesFileBytes);
        if (file.Status ==
            ::AstralChronicle::services::details::LocalTextReadStatus::Succeeded)
        {
            auto parsed = parse(file.Text);
            if (!parsed)
            {
                return false;
            }
            m_savedQueryModels = std::move(*parsed);
            try
            {
                winrt::Windows::Storage::ApplicationData::Current()
                    .LocalSettings()
                    .Values()
                    .Remove(winrt::hstring{ RemoteSavedQueriesLegacySetting });
            }
            catch (...)
            {
            }
            return true;
        }
        if (file.Status ==
            ::AstralChronicle::services::details::LocalTextReadStatus::Failed)
        {
            return false;
        }

        try
        {
            auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto const legacyKey = winrt::hstring{
                RemoteSavedQueriesLegacySetting,
            };
            if (!values.HasKey(legacyKey))
            {
                m_savedQueryModels.clear();
                return true;
            }
            auto const stored =
                winrt::unbox_value<winrt::hstring>(values.Lookup(legacyKey));
            auto parsed = parse(
                std::wstring_view{ stored.c_str(), stored.size() });
            if (!parsed)
            {
                return false;
            }
            m_savedQueryModels = std::move(*parsed);
            if (PersistSavedQueries())
            {
                values.Remove(legacyKey);
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool RemoteViewModel::PersistSavedQueries() const
    {
        try
        {
            std::wstring text;
            for (auto const& profile : m_savedQueryModels)
            {
                if (!text.empty()) text += L'\n';
                text += L"@v2\t" + EscapePersistedField(profile.Name) + L'\t' +
                    EscapePersistedField(profile.Channel) + L'\t' +
                    EscapePersistedField(profile.Query);
            }
            return ::AstralChronicle::services::details::WriteLocalUtf8TextAtomically(
                RemoteSavedQueriesFileName,
                text,
                MaximumRemoteProfilesFileBytes);
        }
        catch (...)
        {
            return false;
        }
    }

    void RemoteViewModel::RebuildSavedQueryView()
    {
        auto values = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& profile : m_savedQueryModels) values.Append(winrt::hstring{ profile.Name + L" — " + profile.Channel });
        m_savedQueries = values;
        RaisePropertyChanged(L"SavedQueries");
    }
}
