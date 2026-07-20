// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "ProvidersViewModel.g.h"
#include "ProviderItemViewModel.h"
#include "Services/IEventProviderService.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstdint>
#include <string>
#include <vector>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct ProvidersViewModel : ProvidersViewModelT<ProvidersViewModel>
    {
        ProvidersViewModel();

        void Initialize(
            ::AstralChronicle::services::IEventProviderService const& providerService,
            ::AstralChronicle::design::IStringResourceService const& strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] bool IsLoading() const noexcept;
        [[nodiscard]] winrt::hstring SearchText() const;
        void SearchText(winrt::hstring const& value);
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::ProviderItemViewModel> Providers() const;
        [[nodiscard]] winrt::AstralChronicle::ProviderItemViewModel SelectedProvider() const;
        void SelectedProvider(winrt::AstralChronicle::ProviderItemViewModel const& value);
        [[nodiscard]] winrt::hstring SelectedProviderName() const;
        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] winrt::hstring SelectionStatus() const;
        [[nodiscard]] winrt::hstring SelectedGuid() const;
        [[nodiscard]] winrt::hstring SelectedResourcePath() const;
        [[nodiscard]] winrt::hstring SelectedParameterPath() const;
        [[nodiscard]] winrt::hstring SelectedMessagePath() const;
        [[nodiscard]] winrt::hstring SelectedHelpLink() const;
        [[nodiscard]] winrt::hstring SelectedMetadataStatus() const;
        [[nodiscard]] winrt::hstring SelectedEventCount() const;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> EventDefinitions() const;
        void Refresh();

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget LoadProvidersAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::wstring searchText);
        winrt::fire_and_forget LoadDetailsAsync(
            std::uint64_t requestVersion,
            ::AstralChronicle::services::QueryCancellation cancellation,
            std::wstring providerName);
        void ApplyProviders(::AstralChronicle::services::EventProviderQueryResult const& result);
        void ApplyDetails(::AstralChronicle::services::EventProviderDetailsResult const& result);
        void ClearDetails();
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();

        ::AstralChronicle::services::IEventProviderService const* m_providerService{};
        ::AstralChronicle::design::IStringResourceService const* m_strings{};
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        ::AstralChronicle::services::QueryCancellation m_cancellation;
        ::AstralChronicle::services::QueryCancellation m_detailsCancellation;
        std::uint64_t m_requestVersion{};
        std::uint64_t m_detailsVersion{};
        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        winrt::hstring m_searchText;
        winrt::hstring m_selectionStatus;
        winrt::hstring m_selectedProviderName;
        winrt::hstring m_selectedGuid;
        winrt::hstring m_selectedResourcePath;
        winrt::hstring m_selectedParameterPath;
        winrt::hstring m_selectedMessagePath;
        winrt::hstring m_selectedHelpLink;
        winrt::hstring m_selectedMetadataStatus;
        winrt::hstring m_selectedEventCount;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::ProviderItemViewModel> m_providers{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_eventDefinitions{ nullptr };
        winrt::AstralChronicle::ProviderItemViewModel m_selectedProvider{ nullptr };
        bool m_hasStatusMessage{ true };
        bool m_isLoading{};
        bool m_hasSelection{};
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct ProvidersViewModel : ProvidersViewModelT<ProvidersViewModel, implementation::ProvidersViewModel>
    {
    };
}
