// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "SessionsPage.g.h"
#include "ViewModels/SessionsViewModel.h"
#include <winrt/Microsoft.UI.h>
#include <memory>
namespace AstralChronicle::services{struct ISessionRepository;} namespace AstralChronicle::design{struct IStringResourceService;}
namespace winrt::AstralChronicle::implementation
{
    struct SessionsPage : SessionsPageT<SessionsPage>{ SessionsPage(); [[nodiscard]] winrt::AstralChronicle::SessionsViewModel ViewModel()const; void Initialize(std::shared_ptr<::AstralChronicle::services::ISessionRepository>,std::shared_ptr<::AstralChronicle::design::IStringResourceService>); void OnNewClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnSaveClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnResumeClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnPinClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnDuplicateClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnArchiveClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnDeleteClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnRefreshClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnExportClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnImportClicked(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);void OnChannelsChanged(winrt::Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);private:winrt::fire_and_forget ExportAsync(winrt::hstring,winrt::Microsoft::UI::WindowId);winrt::fire_and_forget ImportAsync(winrt::Microsoft::UI::WindowId);winrt::AstralChronicle::SessionsViewModel m_viewModel{nullptr};std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;};
}
namespace winrt::AstralChronicle::factory_implementation{struct SessionsPage:SessionsPageT<SessionsPage,implementation::SessionsPage>{};}
