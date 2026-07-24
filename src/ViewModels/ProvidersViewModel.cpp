// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "ProvidersViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "ProvidersViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

    [[nodiscard]] winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity SeverityFor(
        AstralChronicle::services::EventQueryStatus const status)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        return status == Status::AccessDenied || status == Status::ServiceUnavailable ||
            status == Status::Failed || status == Status::InvalidChannel
            ? winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error
            : winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
    }

    [[nodiscard]] winrt::hstring StatusKey(
        AstralChronicle::services::EventQueryStatus const status)
    {
        using Status = AstralChronicle::services::EventQueryStatus;
        switch (status)
        {
        case Status::AccessDenied: return L"Providers.AccessDenied.Text";
        case Status::ServiceUnavailable: return L"Providers.ServiceUnavailable.Text";
        case Status::Cancelled: return L"Providers.Cancelled.Text";
        case Status::NoEvents: return L"Providers.NoProviders.Text";
        case Status::Succeeded: return L"Providers.Ready.Text";
        case Status::InvalidChannel:
        case Status::Failed:
        default: return L"Providers.QueryFailed.Text";
        }
    }
}

namespace winrt::AstralChronicle::implementation
{
    ProvidersViewModel::ProvidersViewModel()
        : m_providers(winrt::single_threaded_observable_vector<winrt::AstralChronicle::ProviderItemViewModel>()),
          m_eventDefinitions(winrt::single_threaded_observable_vector<winrt::hstring>())
    {
    }

    ProvidersViewModel::~ProvidersViewModel() noexcept
    {
        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
    }

    void ProvidersViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventProviderService> providerService,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_providerService = std::move(providerService);
        m_strings = std::move(strings);
        if (!m_providerService || !m_strings)
        {
            throw std::invalid_argument("Providers require provider and string services.");
        }
        m_dispatcher = dispatcher;
        m_heading = m_strings->GetString(L"Providers.Heading");
        m_summary = m_strings->GetString(L"Providers.Summary");
        m_selectionStatus = m_strings->GetString(L"Providers.SelectProvider.Text");
        m_statusText = m_strings->GetString(L"Providers.Loading.Text");
        m_statusDetails = m_strings->GetString(L"Providers.LoadingDetails.Text");
        Refresh();
    }

    void ProvidersViewModel::Refresh()
    {
        if (!m_providerService || !m_strings || !m_dispatcher)
        {
            return;
        }

        if (m_cancellation)
        {
            m_cancellation->store(true, std::memory_order_relaxed);
        }
        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        ++m_detailsVersion;
        m_detailsCancellation.reset();
        m_selectedProvider = nullptr;
        m_hasSelection = false;
        m_selectedProviderName.clear();
        m_selectionStatus = m_strings->GetString(L"Providers.SelectProvider.Text");
        ClearDetails();
        RaisePropertyChanged(L"SelectedProvider");
        RaisePropertyChanged(L"SelectedProviderName");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectionStatus");

        m_cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        auto const requestVersion = ++m_requestVersion;
        m_isLoading = true;
        m_hasStatusMessage = true;
        m_statusSeverity = Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational;
        m_statusText = m_strings->GetString(L"Providers.Loading.Text");
        m_statusDetails = m_strings->GetString(L"Providers.LoadingDetails.Text");
        RaiseStatusProperties();
        LoadProvidersAsync(requestVersion, m_cancellation, std::wstring{ m_searchText.c_str() });
    }

    winrt::fire_and_forget ProvidersViewModel::LoadProvidersAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring searchText)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const providerService = m_providerService;
            auto const dispatcher = m_dispatcher;
            if (!providerService || !dispatcher) co_return;
            co_await winrt::resume_background();
            auto const result = providerService->QueryProviders(searchText, 512, cancellation);
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || requestVersion != strongThis->m_requestVersion || cancellation != strongThis->m_cancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            strongThis->ApplyProviders(result);
        }
        catch (...)
        {
            co_return;
        }
    }

    void ProvidersViewModel::ApplyProviders(
        ::AstralChronicle::services::EventProviderQueryResult const& result)
    {
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::ProviderItemViewModel>();
        for (auto const& provider : result.Providers)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::ProviderItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::ProviderItemViewModel>(item)->Initialize(
                winrt::hstring{ provider.Name });
            values.Append(item);
        }

        m_providers = values;
        m_selectedProvider = nullptr;
        m_hasSelection = false;
        m_selectedProviderName.clear();
        m_selectionStatus = m_strings->GetString(L"Providers.SelectProvider.Text");
        ClearDetails();
        m_isLoading = false;
        m_statusSeverity = SeverityFor(result.Status);
        m_statusText = m_strings->GetString(StatusKey(result.Status));
        m_statusDetails = result.Status == ::AstralChronicle::services::EventQueryStatus::Succeeded ||
            result.Status == ::AstralChronicle::services::EventQueryStatus::NoEvents
            ? winrt::hstring{}
            : FormatResource(m_strings->GetString(L"Providers.ErrorDetails.Text"),
                { winrt::to_hstring(result.ErrorCode) });
        m_hasStatusMessage = result.Status != ::AstralChronicle::services::EventQueryStatus::Succeeded &&
            result.Status != ::AstralChronicle::services::EventQueryStatus::NoEvents;
        m_summary = FormatResource(m_strings->GetString(L"Providers.SummaryCount.Text"),
            { winrt::to_hstring(result.Providers.size()) });
        RaisePropertyChanged(L"Providers");
        RaisePropertyChanged(L"Summary");
        RaisePropertyChanged(L"SelectedProvider");
        RaisePropertyChanged(L"SelectedProviderName");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectionStatus");
        RaiseStatusProperties();
    }

    void ProvidersViewModel::SelectedProvider(
        winrt::AstralChronicle::ProviderItemViewModel const& value)
    {
        if (m_selectedProvider == value)
        {
            return;
        }

        if (m_detailsCancellation)
        {
            m_detailsCancellation->store(true, std::memory_order_relaxed);
        }
        m_selectedProvider = value;
        m_hasSelection = value != nullptr;
        m_selectedProviderName = value ? value.Name() : winrt::hstring{};
        m_selectionStatus = value ? winrt::hstring{} : m_strings->GetString(L"Providers.SelectProvider.Text");
        ++m_detailsVersion;
        ClearDetails();
        RaisePropertyChanged(L"SelectedProvider");
        RaisePropertyChanged(L"SelectedProviderName");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectionStatus");
        if (!value || !m_providerService || !m_strings || !m_dispatcher)
        {
            return;
        }

        auto const cancellation = ::AstralChronicle::services::MakeQueryCancellation();
        m_detailsCancellation = cancellation;
        m_selectedMetadataStatus = m_strings->GetString(L"Providers.MetadataLoading.Text");
        RaisePropertyChanged(L"SelectedMetadataStatus");
        LoadDetailsAsync(m_detailsVersion, cancellation, std::wstring{ value.Name().c_str() });
    }

    winrt::fire_and_forget ProvidersViewModel::LoadDetailsAsync(
        std::uint64_t const requestVersion,
        ::AstralChronicle::services::QueryCancellation cancellation,
        std::wstring providerName)
    {
        try
        {
            auto const weakThis = get_weak();
            auto const providerService = m_providerService;
            auto const dispatcher = m_dispatcher;
            if (!providerService || !dispatcher) co_return;
            co_await winrt::resume_background();
            auto const result = providerService->QueryDetails(providerName, cancellation);
            co_await wil::resume_foreground(dispatcher);
            auto const strongThis = weakThis.get();
            if (!strongThis || requestVersion != strongThis->m_detailsVersion || cancellation != strongThis->m_detailsCancellation ||
                cancellation->load(std::memory_order_relaxed))
            {
                co_return;
            }
            strongThis->ApplyDetails(result);
        }
        catch (...)
        {
            co_return;
        }
    }

    void ProvidersViewModel::ApplyDetails(
        ::AstralChronicle::services::EventProviderDetailsResult const& result)
    {
        if (result.Status != ::AstralChronicle::services::EventQueryStatus::Succeeded)
        {
            m_selectedMetadataStatus = m_strings->GetString(StatusKey(result.Status));
            RaisePropertyChanged(L"SelectedMetadataStatus");
            return;
        }

        m_selectedGuid = result.Details.Guid;
        m_selectedResourcePath = result.Details.ResourceFilePath;
        m_selectedParameterPath = result.Details.ParameterFilePath;
        m_selectedMessagePath = result.Details.MessageFilePath;
        m_selectedHelpLink = result.Details.HelpLink;
        m_selectedMetadataStatus = result.Details.MetadataAvailable
            ? m_strings->GetString(L"Providers.MetadataAvailable.Text")
            : m_strings->GetString(L"Providers.MetadataUnavailable.Text");
        m_selectedEventCount = FormatResource(m_strings->GetString(L"Providers.EventCount.Text"),
            { winrt::to_hstring(result.Details.EventDefinitions.size()) });

        auto definitions = winrt::single_threaded_observable_vector<winrt::hstring>();
        for (auto const& definition : result.Details.EventDefinitions)
        {
            definitions.Append(FormatResource(m_strings->GetString(L"Providers.Definition.Text"),
                { winrt::to_hstring(definition.EventId),
                  winrt::to_hstring(definition.Version),
                  winrt::to_hstring(definition.Level),
                  winrt::to_hstring(definition.Channel),
                  winrt::to_hstring(definition.Task),
                  winrt::to_hstring(definition.Opcode),
                  winrt::to_hstring(definition.Keyword),
                  winrt::hstring{ definition.Template } }));
        }
        m_eventDefinitions = definitions;

        RaisePropertyChanged(L"SelectedGuid");
        RaisePropertyChanged(L"SelectedResourcePath");
        RaisePropertyChanged(L"SelectedParameterPath");
        RaisePropertyChanged(L"SelectedMessagePath");
        RaisePropertyChanged(L"SelectedHelpLink");
        RaisePropertyChanged(L"SelectedMetadataStatus");
        RaisePropertyChanged(L"SelectedEventCount");
        RaisePropertyChanged(L"EventDefinitions");
    }

    void ProvidersViewModel::ClearDetails()
    {
        m_selectedGuid.clear();
        m_selectedResourcePath.clear();
        m_selectedParameterPath.clear();
        m_selectedMessagePath.clear();
        m_selectedHelpLink.clear();
        m_selectedEventCount.clear();
        m_selectedMetadataStatus = m_hasSelection && m_strings
            ? m_strings->GetString(L"Providers.MetadataLoading.Text")
            : winrt::hstring{};
        m_eventDefinitions = winrt::single_threaded_observable_vector<winrt::hstring>();
        RaisePropertyChanged(L"SelectedGuid");
        RaisePropertyChanged(L"SelectedResourcePath");
        RaisePropertyChanged(L"SelectedParameterPath");
        RaisePropertyChanged(L"SelectedMessagePath");
        RaisePropertyChanged(L"SelectedHelpLink");
        RaisePropertyChanged(L"SelectedMetadataStatus");
        RaisePropertyChanged(L"SelectedEventCount");
        RaisePropertyChanged(L"EventDefinitions");
    }

    winrt::hstring ProvidersViewModel::Heading() const { return m_heading; }
    winrt::hstring ProvidersViewModel::Summary() const { return m_summary; }
    winrt::hstring ProvidersViewModel::StatusText() const { return m_statusText; }
    winrt::hstring ProvidersViewModel::StatusDetails() const { return m_statusDetails; }
    Microsoft::UI::Xaml::Controls::InfoBarSeverity ProvidersViewModel::StatusSeverity() const noexcept { return m_statusSeverity; }
    bool ProvidersViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    bool ProvidersViewModel::IsLoading() const noexcept { return m_isLoading; }
    winrt::hstring ProvidersViewModel::SearchText() const { return m_searchText; }
    void ProvidersViewModel::SearchText(winrt::hstring const& value)
    {
        if (m_searchText != value)
        {
            m_searchText = value;
            RaisePropertyChanged(L"SearchText");
        }
    }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::ProviderItemViewModel> ProvidersViewModel::Providers() const { return m_providers; }
    winrt::AstralChronicle::ProviderItemViewModel ProvidersViewModel::SelectedProvider() const { return m_selectedProvider; }
    winrt::hstring ProvidersViewModel::SelectedProviderName() const { return m_selectedProviderName; }
    bool ProvidersViewModel::HasSelection() const noexcept { return m_hasSelection; }
    winrt::hstring ProvidersViewModel::SelectionStatus() const { return m_selectionStatus; }
    winrt::hstring ProvidersViewModel::SelectedGuid() const { return m_selectedGuid; }
    winrt::hstring ProvidersViewModel::SelectedResourcePath() const { return m_selectedResourcePath; }
    winrt::hstring ProvidersViewModel::SelectedParameterPath() const { return m_selectedParameterPath; }
    winrt::hstring ProvidersViewModel::SelectedMessagePath() const { return m_selectedMessagePath; }
    winrt::hstring ProvidersViewModel::SelectedHelpLink() const { return m_selectedHelpLink; }
    winrt::hstring ProvidersViewModel::SelectedMetadataStatus() const { return m_selectedMetadataStatus; }
    winrt::hstring ProvidersViewModel::SelectedEventCount() const { return m_selectedEventCount; }
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> ProvidersViewModel::EventDefinitions() const { return m_eventDefinitions; }

    void ProvidersViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"StatusDetails");
        RaisePropertyChanged(L"StatusSeverity");
        RaisePropertyChanged(L"HasStatusMessage");
        RaisePropertyChanged(L"IsLoading");
    }

    winrt::event_token ProvidersViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void ProvidersViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void ProvidersViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
