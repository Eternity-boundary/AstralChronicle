// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "ProvidersPage.xaml.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "ProvidersPage.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>

#include <chrono>
#include <string>

namespace
{
    [[nodiscard]] std::wstring EscapeXPathLiteral(std::wstring value)
    {
        if (value.find(L'\'') == std::wstring::npos)
        {
            return L"'" + value + L"'";
        }
        if (value.find(L'\"') == std::wstring::npos)
        {
            return L"\"" + value + L"\"";
        }

        std::wstring expression{ L"concat(" };
        std::size_t start{};
        bool first = true;
        for (;;)
        {
            auto const apostrophe = value.find(L'\'', start);
            if (!first)
            {
                expression += L",";
            }
            expression += L"'" + value.substr(
                start,
                apostrophe == std::wstring::npos ? std::wstring::npos : apostrophe - start) + L"'";
            first = false;

            if (apostrophe == std::wstring::npos)
            {
                break;
            }

            expression += L",\"'\"";
            start = apostrophe + 1;
        }
        expression += L")";
        return expression;
    }
}

namespace winrt::AstralChronicle::implementation
{
    ProvidersPage::ProvidersPage()
        : m_viewModel(winrt::make<ProvidersViewModel>())
    {
        InitializeComponent();
    }

    ProvidersPage::~ProvidersPage()
    {
        if (m_searchDebounceTimer)
        {
            m_searchDebounceTimer.Stop();
        }
        m_searchDebounceTickRevoker.revoke();
    }

    winrt::AstralChronicle::ProvidersViewModel ProvidersPage::ViewModel() const
    {
        return m_viewModel;
    }

    void ProvidersPage::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventProviderService> providerService,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        ::AstralChronicle::navigation::INavigationService& navigation,
        std::function<void(std::wstring_view)> navigationSelectionChanged)
    {
        m_navigation = &navigation;
        m_navigationSelectionChanged = std::move(navigationSelectionChanged);
        Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            ProviderSearchBox(),
            strings->GetString(L"ProvidersSearchBox.PlaceholderText"));
        if (!m_searchDebounceTimer)
        {
            m_searchDebounceTimer = PageRoot().DispatcherQueue().CreateTimer();
            m_searchDebounceTimer.Interval(std::chrono::milliseconds{ 275 });
            m_searchDebounceTimer.IsRepeating(false);
            m_searchDebounceTickRevoker = m_searchDebounceTimer.Tick(
                winrt::auto_revoke,
                [this](Microsoft::UI::Dispatching::DispatcherQueueTimer const&,
                    winrt::Windows::Foundation::IInspectable const&)
                {
                    ApplyPendingSearch();
                });
        }
        winrt::get_self<ProvidersViewModel>(m_viewModel)->Initialize(
            std::move(providerService),
            std::move(strings),
            PageRoot().DispatcherQueue());
    }

    void ProvidersPage::OnRefreshClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<ProvidersViewModel>(m_viewModel)->Refresh();
    }

    void ProvidersPage::OnSearchTextChanged(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args)
    {
        if (args.Reason() != Microsoft::UI::Xaml::Controls::AutoSuggestionBoxTextChangeReason::UserInput)
        {
            return;
        }
        if (!m_searchDebounceTimer)
        {
            ApplyPendingSearch();
            return;
        }

        m_searchDebounceTimer.Stop();
        m_searchDebounceTimer.Start();
    }

    void ProvidersPage::ApplyPendingSearch()
    {
        winrt::get_self<ProvidersViewModel>(m_viewModel)->SearchText(ProviderSearchBox().Text());
        winrt::get_self<ProvidersViewModel>(m_viewModel)->Refresh();
    }

    winrt::hstring ProvidersPage::BuildCopyText() const
    {
        auto const viewModel = winrt::get_self<ProvidersViewModel>(m_viewModel);
        std::wstring text;
        text += L"Provider: " + std::wstring{ viewModel->SelectedProviderName().c_str() } + L"\r\n";
        text += L"GUID: " + std::wstring{ viewModel->SelectedGuid().c_str() } + L"\r\n";
        text += L"Resource: " + std::wstring{ viewModel->SelectedResourcePath().c_str() } + L"\r\n";
        text += L"Parameter: " + std::wstring{ viewModel->SelectedParameterPath().c_str() } + L"\r\n";
        text += L"Message: " + std::wstring{ viewModel->SelectedMessagePath().c_str() } + L"\r\n";
        text += L"Help: " + std::wstring{ viewModel->SelectedHelpLink().c_str() } + L"\r\n";
        for (auto const& definition : viewModel->EventDefinitions())
        {
            text += std::wstring{ definition.c_str() } + L"\r\n";
        }
        return winrt::hstring{ text };
    }

    void ProvidersPage::OnCopyClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const text = BuildCopyText();
        if (text.empty())
        {
            return;
        }
        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(text);
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
    }

    void ProvidersPage::OnOpenEventsClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const name = std::wstring{ winrt::get_self<ProvidersViewModel>(m_viewModel)->SelectedProviderName().c_str() };
        if (name.empty() || !m_navigation)
        {
            return;
        }
        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Query = L"*[System[Provider[@Name=" + EscapeXPathLiteral(name) + L"]]]";
        if (m_navigation->Navigate(request) && m_navigationSelectionChanged)
        {
            m_navigationSelectionChanged(request.Route);
        }
    }

    void ProvidersPage::OnExportClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const text = BuildCopyText();
        if (!text.empty())
        {
            ExportAsync(text);
        }
    }

    winrt::fire_and_forget ProvidersPage::ExportAsync(winrt::hstring text)
    {
        try
        {
            auto lifetime = get_strong();
            co_await winrt::resume_background();
            auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto const file = co_await folder.CreateFileAsync(
                L"EventProvider-metadata.txt",
                winrt::Windows::Storage::CreationCollisionOption::GenerateUniqueName);
            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, text);
        }
        catch (...)
        {
            // Export is best effort; provider inspection remains available if storage is unavailable.
        }
    }
}
