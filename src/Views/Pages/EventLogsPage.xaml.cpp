// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "EventLogsPage.xaml.h"

#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/ElevationRestart.h"
#include "ViewModels/PersistedSettings.h"

#include "EventLogsPage.g.cpp"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace
{
    template <typename T>
    [[nodiscard]] T FindVisualDescendant(winrt::Microsoft::UI::Xaml::DependencyObject const& root)
    {
        auto const childCount = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (std::int32_t index = 0; index < childCount; ++index)
        {
            auto const child = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(root, index);
            if (auto const match = child.try_as<T>())
            {
                return match;
            }
            if (auto const descendant = FindVisualDescendant<T>(child))
            {
                return descendant;
            }
        }
        return nullptr;
    }

}

namespace winrt::AstralChronicle::implementation
{
    EventLogsPage::EventLogsPage()
        : m_viewModel(winrt::make<EventLogsViewModel>())
    {
        InitializeComponent();
    }

    EventLogsPage::~EventLogsPage()
    {
        if (m_viewModel && m_viewModelPropertyChangedToken.value != 0)
        {
            m_viewModel.PropertyChanged(m_viewModelPropertyChangedToken);
        }
    }

    winrt::AstralChronicle::EventLogsViewModel EventLogsPage::ViewModel() const
    {
        return m_viewModel;
    }

    void EventLogsPage::Initialize(
        std::shared_ptr<::AstralChronicle::services::IEventQueryService> eventQuery,
        std::shared_ptr<::AstralChronicle::services::IEventBookmarkStore> bookmarkStore,
        std::shared_ptr<::AstralChronicle::services::ITextExportService> textExporter,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        std::optional<::AstralChronicle::models::EventChannelIdentifier> const& channel,
        std::optional<std::wstring> const& query,
        std::optional<std::wstring> const& searchText)
    {
        m_strings = strings;
        Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            EventSearchBox(),
            strings->GetString(L"EventLogsSearchBox.PlaceholderText"));
        if (m_viewModelPropertyChangedToken.value != 0)
        {
            m_viewModel.PropertyChanged(m_viewModelPropertyChangedToken);
        }
        auto const weak = get_weak();
        m_viewModelPropertyChangedToken = m_viewModel.PropertyChanged(
            [weak](winrt::Windows::Foundation::IInspectable const&,
                Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
            {
                auto const self = weak.get();
                if (!self)
                {
                    return;
                }
                if (args.PropertyName() == L"IsAccessDenied")
                {
                    self->UpdateAccessDeniedAction();
                }
                else if (args.PropertyName() == L"CanCloseActiveContext")
                {
                    self->UpdateCloseContextAction();
                }
                else if (args.PropertyName() == L"SortKey" || args.PropertyName() == L"SortAscending")
                {
                    self->UpdateSortAutomation();
                }
            });
        UpdateAccessDeniedAction();

        auto const settings =
            ::AstralChronicle::viewmodels::PersistedSettingsSnapshot::Load();
        m_detailsPaneVisible = settings.DetailsPaneOpen;
        m_narrowDetailsPaneVisible = settings.DetailsPaneOpen;
        winrt::get_self<EventLogsViewModel>(m_viewModel)->Initialize(
            std::move(eventQuery),
            std::move(bookmarkStore),
            std::move(textExporter),
            std::move(strings),
            PageRoot().DispatcherQueue(),
            channel,
            query,
            searchText);
        UpdateCloseContextAction();
        UpdateSortAutomation();
        UpdateResponsiveLayout(ContentGrid().ActualWidth());
    }

    void EventLogsPage::UpdateAccessDeniedAction()
    {
        RestartAsAdministratorButton().Visibility(
            m_viewModel.IsAccessDenied()
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
    }

    void EventLogsPage::UpdateCloseContextAction()
    {
        CloseContextCommand().Visibility(
            winrt::get_self<EventLogsViewModel>(m_viewModel)->CanCloseActiveContext()
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
    }

    void EventLogsPage::OnRefreshClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->Refresh();
    }

    void EventLogsPage::OnCloseActiveContextClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<EventLogsViewModel>(m_viewModel)->CloseActiveContext();
    }

    void EventLogsPage::OnRestartAsAdministratorClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (::AstralChronicle::services::RestartElevatedApplication())
        {
            Microsoft::UI::Xaml::Application::Current().Exit();
        }
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

    void EventLogsPage::UpdateSortAutomation()
    {
        if (!m_strings)
        {
            return;
        }

        auto const headers = std::array<Microsoft::UI::Xaml::FrameworkElement, 9>{
            EventLevelHeaderButton(),
            EventTimeHeaderButton(),
            EventProviderHeaderButton(),
            EventIdHeaderButton(),
            EventTaskHeaderButton(),
            EventChannelHeaderButton(),
            EventUserHeaderButton(),
            EventComputerHeaderButton(),
            EventDescriptionHeaderButton(),
        };
        auto const sortKey = m_viewModel.SortKey();
        auto const sortState = m_strings->GetString(
            m_viewModel.SortAscending()
                ? L"EventLogs.SortAscending.Text"
                : L"EventLogs.SortDescending.Text");

        for (auto const& header : headers)
        {
            auto const key = winrt::unbox_value<winrt::hstring>(header.Tag());
            Microsoft::UI::Xaml::Automation::AutomationProperties::SetItemStatus(
                header,
                key == sortKey ? sortState : winrt::hstring{});
        }
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
        auto const windowId = PageRoot().XamlRoot().ContentIslandEnvironment().AppWindowId();
        winrt::get_self<EventLogsViewModel>(m_viewModel)->ExportSelectedEvents(windowId);
    }

    void EventLogsPage::OnOpenSavedLogClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_isPickingSavedLog)
        {
            return;
        }

        m_isPickingSavedLog = true;
        OpenSavedLogAsync();
    }

    winrt::fire_and_forget EventLogsPage::OpenSavedLogAsync()
    {
        auto lifetime = get_strong();
        try
        {
            auto const windowId = PageRoot().XamlRoot().ContentIslandEnvironment().AppWindowId();
            winrt::Microsoft::Windows::Storage::Pickers::FileOpenPicker picker{ windowId };
            picker.FileTypeFilter().Append(L".evtx");
            auto const result = co_await picker.PickSingleFileAsync();
            if (result)
            {
                winrt::get_self<EventLogsViewModel>(m_viewModel)->OpenSavedLog(
                    std::wstring{ result.Path().c_str() });
            }
        }
        catch (...)
        {
            // The picker can fail before showing if its hosting window is closing.
        }

        m_isPickingSavedLog = false;
    }

    void EventLogsPage::OnToggleDetailsClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const width = ContentGrid().ActualWidth();
        if (width < 800.0)
        {
            m_narrowDetailsPaneVisible = !m_narrowDetailsPaneVisible;
        }
        else
        {
            m_detailsPaneVisible = !m_detailsPaneVisible;
        }
        UpdateResponsiveLayout(width);
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
        if (columns.Size() < 2 || rows.Size() < 2)
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
        auto const wideDetails = Microsoft::UI::Xaml::GridLengthHelper::FromPixels(400.0);
        auto const mediumDetails = Microsoft::UI::Xaml::GridLengthHelper::FromPixels(360.0);

        if (width >= 1200.0)
        {
            columns.GetAt(0).Width(star);
            columns.GetAt(1).Width(m_detailsPaneVisible ? wideDetails : zero);
            rows.GetAt(0).Height(star);
            rows.GetAt(1).Height(zero);
            EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            DetailsPane().Visibility(m_detailsPaneVisible
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 1);
            ToggleDetailsCommand().IsChecked(
                winrt::box_value(m_detailsPaneVisible).as<winrt::Windows::Foundation::IReference<bool>>());
            return;
        }

        if (width >= 800.0)
        {
            columns.GetAt(0).Width(star);
            columns.GetAt(1).Width(m_detailsPaneVisible ? mediumDetails : zero);
            rows.GetAt(0).Height(star);
            rows.GetAt(1).Height(zero);
            EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            DetailsPane().Visibility(m_detailsPaneVisible
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed);
            Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 0);
            Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 1);
            ToggleDetailsCommand().IsChecked(
                winrt::box_value(m_detailsPaneVisible).as<winrt::Windows::Foundation::IReference<bool>>());
            return;
        }

        auto const detailsVisible = m_narrowDetailsPaneVisible;
        columns.GetAt(0).Width(star);
        columns.GetAt(1).Width(zero);
        rows.GetAt(0).Height(detailsVisible ? threeStar : star);
        rows.GetAt(1).Height(detailsVisible ? twoStar : zero);
        EventListPane().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        DetailsPane().Visibility(detailsVisible
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed);
        Microsoft::UI::Xaml::Controls::Grid::SetRow(DetailsPane(), 1);
        Microsoft::UI::Xaml::Controls::Grid::SetColumn(DetailsPane(), 0);
        ToggleDetailsCommand().IsChecked(
            winrt::box_value(detailsVisible).as<winrt::Windows::Foundation::IReference<bool>>());
    }

    void EventLogsPage::OnEventListLoaded(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const scrollViewer = FindVisualDescendant<Microsoft::UI::Xaml::Controls::ScrollViewer>(EventList());
        if (!scrollViewer || scrollViewer == m_eventListScrollViewer)
        {
            return;
        }

        m_eventListViewChangedRevoker.revoke();
        m_eventListScrollViewer = scrollViewer;
        m_eventListViewChangedRevoker = scrollViewer.ViewChanged(
            winrt::auto_revoke,
            { this, &EventLogsPage::OnEventListViewChanged });
        OnEventListViewChanged(scrollViewer, nullptr);
    }

    void EventLogsPage::OnEventListViewChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args)
    {
        auto const scrollViewer = sender.try_as<Microsoft::UI::Xaml::Controls::ScrollViewer>();
        if (!scrollViewer)
        {
            return;
        }

        auto const horizontalOffset = winrt::box_value(scrollViewer.HorizontalOffset())
            .as<winrt::Windows::Foundation::IReference<double>>();
        (void)EventHeaderScrollViewer().ChangeView(horizontalOffset, nullptr, nullptr, true);

        if (!args || args.IsIntermediate() || scrollViewer.VerticalOffset() <= 0.0)
        {
            return;
        }

        auto const prefetchDistance = std::max(240.0, scrollViewer.ViewportHeight() * 1.5);
        if (scrollViewer.ScrollableHeight() - scrollViewer.VerticalOffset() <= prefetchDistance)
        {
            winrt::get_self<EventLogsViewModel>(m_viewModel)->LoadMore();
        }
    }

    void EventLogsPage::OnEventItemContextRequested(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::ContextRequestedEventArgs const&)
    {
        auto const element = sender.try_as<Microsoft::UI::Xaml::FrameworkElement>();
        auto const item = element
            ? element.DataContext().try_as<winrt::AstralChronicle::EventLogItemViewModel>()
            : nullptr;
        if (!item)
        {
            return;
        }

        auto const selectedItems = EventList().SelectedItems();
        for (auto const& selectedValue : selectedItems)
        {
            if (selectedValue.try_as<winrt::AstralChronicle::EventLogItemViewModel>() == item)
            {
                return;
            }
        }

        selectedItems.Clear();
        selectedItems.Append(item);
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
