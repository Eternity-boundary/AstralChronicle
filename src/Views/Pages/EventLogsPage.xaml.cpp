// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "EventLogsPage.xaml.h"

#include "EventLogsPage.g.cpp"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::AstralChronicle::implementation
{
    EventLogsPage::EventLogsPage()
        : m_viewModel(winrt::make<EventLogsViewModel>())
    {
        InitializeComponent();
    }

    winrt::AstralChronicle::EventLogsViewModel EventLogsPage::ViewModel() const
    {
        return m_viewModel;
    }

    void EventLogsPage::Initialize(
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel,
        std::optional<std::wstring> const& query)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->Initialize(
            eventQuery,
            strings,
            PageRoot().DispatcherQueue(),
            channel,
            query);
    }

    void EventLogsPage::OnRefreshClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->Refresh();
    }

    void EventLogsPage::OnClearFilterClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ClearFilter();
    }

    void EventLogsPage::OnCopyClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const text = winrt::get_self<EventLogsViewModel>(m_viewModel)->CopySelectedEventsText();
        if (text.empty())
        {
            return;
        }

        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(text);
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
    }

    void EventLogsPage::OnSortClicked(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const button = sender.as<Microsoft::UI::Xaml::FrameworkElement>();
        auto const key = winrt::unbox_value<winrt::hstring>(button.Tag());
        winrt::get_self<EventLogsViewModel>(m_viewModel)->SortBy(key);
    }

    void EventLogsPage::OnApplyStructuredFilterClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ApplyStructuredFilter();
        FilterFlyout().Hide();
    }

    void EventLogsPage::OnClearStructuredFilterClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ClearStructuredFilter();
        FilterFlyout().Hide();
    }

    void EventLogsPage::OnFilterLevelChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        if (!combo.SelectedValue())
        {
            return;
        }
        try
        {
            winrt::get_self<EventLogsViewModel>(m_viewModel)->FilterLevel(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
            // The selection is controlled by localized ComboBoxItems; ignore a transient unset value.
        }
    }

    void EventLogsPage::OnBookmarkClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ToggleBookmarks();
    }

    void EventLogsPage::OnExportClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ExportSelectedEvents();
    }

    void EventLogsPage::OnToggleDetailsClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        m_detailsPaneVisible = !m_detailsPaneVisible;
        UpdateResponsiveLayout(ContentGrid().ActualWidth());
    }

    void EventLogsPage::OnContentGridSizeChanged(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
    {
        UpdateResponsiveLayout(args.NewSize().Width);
    }

    void EventLogsPage::UpdateResponsiveLayout(double const width)
    {
        auto const columns = ContentGrid().ColumnDefinitions();
        auto const rows = ContentGrid().RowDefinitions();
        if (columns.Size() < 3 || rows.Size() < 2)
        {
            return;
        }

        auto const star = Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(
            1.0,
            Microsoft::UI::Xaml::GridUnitType::Star);
        auto const zero = Microsoft::UI::Xaml::GridLengthHelper::FromPixels(0.0);
        auto const details = Microsoft::UI::Xaml::GridLengthHelper::FromPixels(480.0);

        if (width >= 1200.0)
        {
            columns.GetAt(0).Width(zero);
            columns.GetAt(1).Width(star);
            columns.GetAt(2).Width(m_detailsPaneVisible ? details : zero);
            rows.GetAt(0).Height(star);
            rows.GetAt(1).Height(zero);
            ChannelPane().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            DetailsPane().Visibility(m_detailsPaneVisible
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 2);
            return;
        }

        if (width >= 800.0)
        {
            columns.GetAt(0).Width(zero);
            columns.GetAt(1).Width(star);
            columns.GetAt(2).Width(m_detailsPaneVisible ? details : zero);
            rows.GetAt(0).Height(star);
            rows.GetAt(1).Height(zero);
            ChannelPane().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            DetailsPane().Visibility(m_detailsPaneVisible
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 2);
            return;
        }

        columns.GetAt(0).Width(zero);
        columns.GetAt(1).Width(star);
        columns.GetAt(2).Width(zero);
        rows.GetAt(0).Height(star);
        rows.GetAt(1).Height(m_detailsPaneVisible ? details : zero);
        ChannelPane().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        DetailsPane().Visibility(m_detailsPaneVisible
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed);
        Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 1);
        Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 1);
    }

    void EventLogsPage::OnSelectionChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
    {
        auto const list = sender.as<Microsoft::UI::Xaml::Controls::ListView>();
        std::vector<winrt::AstralChronicle::EventLogItemViewModel> selected;
        for (auto const& value : list.SelectedItems())
        {
            auto const item = value.try_as<winrt::AstralChronicle::EventLogItemViewModel>();
            if (item)
            {
                selected.emplace_back(item);
            }
        }
        winrt::AstralChronicle::EventLogItemViewModel lastAdded{ nullptr };
        for (auto const& value : args.AddedItems())
        {
            auto const item = value.try_as<winrt::AstralChronicle::EventLogItemViewModel>();
            if (item)
            {
                lastAdded = item;
            }
        }

        winrt::get_self<EventLogsViewModel>(m_viewModel)->SetSelectedEvents(selected, lastAdded);
    }
}
