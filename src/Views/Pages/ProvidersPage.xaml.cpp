// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "ProvidersPage.xaml.h"

#include "ProvidersPage.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>

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
        bool first{};
        for (std::size_t index{}; index <= value.size(); ++index)
        {
            if (index == value.size() || value[index] == L'\'' || value[index] == L'\"')
            {
                if (!first)
                {
                    first = true;
                }
                else
                {
                    expression += L",";
                }
                expression += L"'" + value.substr(start, index - start) + L"'";
                if (index < value.size())
                {
                    expression += L",\"";
                    expression += value[index];
                    expression += L"\"";
                    start = index + 1;
                }
            }
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

    winrt::AstralChronicle::ProvidersViewModel ProvidersPage::ViewModel() const
    {
        return m_viewModel;
    }

    void ProvidersPage::Initialize(
        ::AstralChronicle::services::IEventProviderService const& providerService,
        ::AstralChronicle::design::IStringResourceService const& strings,
        ::AstralChronicle::navigation::INavigationService& navigation)
    {
        m_navigation = &navigation;
        winrt::get_self<ProvidersViewModel>(m_viewModel)->Initialize(
            providerService,
            strings,
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
        (void)m_navigation->Navigate(request);
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
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        try
        {
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
