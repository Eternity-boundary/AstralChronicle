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
        std::shared_ptr<::AstralChronicle::services::IEventLiveDataService> liveEventData,
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings)
    {
        winrt::get_self<LiveViewModel>(m_viewModel)->Initialize(
            std::move(liveService),
            std::move(liveEventData),
            std::move(eventQuery),
            std::move(strings),
            PageRoot().DispatcherQueue());
        UpdateResponsiveLayout(LiveContentGrid().ActualWidth());
    }
    void LivePage::OnStartClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Start(); }
    void LivePage::OnPauseClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Pause(); }
    void LivePage::OnResumeClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Resume(); }
    void LivePage::OnStopClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Stop(); }
    void LivePage::OnClearClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Clear(); }
    void LivePage::OnRecordClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->ToggleRecording(); }
    void LivePage::OnExportClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->Export(); }
    void LivePage::OnBookmarkClicked(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&) { winrt::get_self<LiveViewModel>(m_viewModel)->BookmarkLatest(); }

    void LivePage::OnSelectionChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const selected = sender.as<Microsoft::UI::Xaml::Controls::ListView>()
            .SelectedItem()
            .try_as<winrt::AstralChronicle::EventLogItemViewModel>();
        winrt::get_self<LiveViewModel>(m_viewModel)->SelectedEvent(selected);
    }

    void LivePage::OnContentGridSizeChanged(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
    {
        UpdateResponsiveLayout(args.NewSize().Width);
    }

    void LivePage::UpdateResponsiveLayout(double const width)
    {
        auto const columns = LiveContentGrid().ColumnDefinitions();
        auto const rows = LiveContentGrid().RowDefinitions();
        if (columns.Size() < 3 || rows.Size() < 3)
        {
            return;
        }

        auto const star = Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(
            1.0,
            Microsoft::UI::Xaml::GridUnitType::Star);
        auto const twoStar = Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(
            2.0,
            Microsoft::UI::Xaml::GridUnitType::Star);
        auto const threeStar = Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(
            3.0,
            Microsoft::UI::Xaml::GridUnitType::Star);
        auto const zero = Microsoft::UI::Xaml::GridLengthHelper::FromPixels(0.0);

        LiveConfigurationPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        LiveEventsPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        LiveDetailsPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);

        if (width >= 1200.0)
        {
            columns.GetAt(0).Width(Microsoft::UI::Xaml::GridLengthHelper::FromPixels(280.0));
            columns.GetAt(1).Width(star);
            columns.GetAt(2).Width(Microsoft::UI::Xaml::GridLengthHelper::FromPixels(380.0));
            rows.GetAt(0).Height(zero);
            rows.GetAt(1).Height(star);
            rows.GetAt(2).Height(zero);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveConfigurationPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveConfigurationPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetRowSpan(LiveConfigurationPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveConfigurationPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveEventsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveEventsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveEventsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveDetailsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveDetailsPane(), 2);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveDetailsPane(), 1);
            return;
        }

        if (width >= 800.0)
        {
            columns.GetAt(0).Width(Microsoft::UI::Xaml::GridLengthHelper::FromPixels(260.0));
            columns.GetAt(1).Width(star);
            columns.GetAt(2).Width(zero);
            rows.GetAt(0).Height(zero);
            rows.GetAt(1).Height(threeStar);
            rows.GetAt(2).Height(twoStar);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveConfigurationPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveConfigurationPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetRowSpan(LiveConfigurationPane(), 2);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveConfigurationPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveEventsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveEventsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveEventsPane(), 2);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveDetailsPane(), 2);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveDetailsPane(), 1);
            Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveDetailsPane(), 2);
            return;
        }

        columns.GetAt(0).Width(star);
        columns.GetAt(1).Width(zero);
        columns.GetAt(2).Width(zero);
        rows.GetAt(0).Height(Microsoft::UI::Xaml::GridLengthHelper::FromPixels(260.0));
        rows.GetAt(1).Height(threeStar);
        rows.GetAt(2).Height(twoStar);
        Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveConfigurationPane(), 0);
        Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveConfigurationPane(), 0);
        Microsoft::UI::Xaml::Controls::Grid::SetRowSpan(LiveConfigurationPane(), 1);
        Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveConfigurationPane(), 3);
        Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveEventsPane(), 1);
        Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveEventsPane(), 0);
        Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveEventsPane(), 3);
        Microsoft::UI::Xaml::Controls::Grid::SetRow(LiveDetailsPane(), 2);
        Microsoft::UI::Xaml::Controls::Grid::SetColumn(LiveDetailsPane(), 0);
        Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(LiveDetailsPane(), 3);
    }

    void LivePage::OnUnloaded(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<LiveViewModel>(m_viewModel)->Shutdown();
    }
}
