// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "LivePage.xaml.h"

#include "LivePage.g.cpp"

namespace winrt::AstralChronicle::implementation
{
    LivePage::LivePage() : m_viewModel(winrt::make<LiveViewModel>()) { InitializeComponent(); }
    winrt::AstralChronicle::LiveViewModel LivePage::ViewModel() const { return m_viewModel; }
    void LivePage::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventLiveService> liveService,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings)
    {
        winrt::get_self<LiveViewModel>(m_viewModel)->Initialize(
            std::move(liveService),
            std::move(strings),
            PageRoot().DispatcherQueue());
    }
    void LivePage::OnStartClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Start(); }
    void LivePage::OnPauseClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Pause(); }
    void LivePage::OnResumeClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Resume(); }
    void LivePage::OnStopClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Stop(); }
    void LivePage::OnClearClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Clear(); }
    void LivePage::OnRecordClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->ToggleRecording(); }
    void LivePage::OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Export(); }
    void LivePage::OnBookmarkClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->BookmarkLatest(); }
    void LivePage::OnUnloaded(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<LiveViewModel>(m_viewModel)->Shutdown();
    }
    void LivePage::OnChannelChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&) {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        if (!combo.SelectedValue()) return;
        try { winrt::get_self<LiveViewModel>(m_viewModel)->Channel(winrt::unbox_value<winrt::hstring>(combo.SelectedValue())); }
        catch (...) { }
    }
}
