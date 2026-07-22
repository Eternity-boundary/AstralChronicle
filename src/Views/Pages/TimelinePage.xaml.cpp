// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "TimelinePage.xaml.h"

#include "TimelinePage.g.cpp"

#include <cwchar>
#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Storage.h>

namespace winrt::AstralChronicle::implementation
{
    TimelinePage::TimelinePage()
        : m_viewModel(winrt::make<TimelineViewModel>())
    {
        InitializeComponent();
    }

    winrt::AstralChronicle::TimelineViewModel TimelinePage::ViewModel() const
    {
        return m_viewModel;
    }

    void TimelinePage::Initialize(
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
        ::AstralChronicle::navigation::INavigationService& navigation,
        std::function<void(std::wstring_view)> navigationSelectionChanged)
    {
        m_navigation = &navigation;
        m_navigationSelectionChanged = std::move(navigationSelectionChanged);
        winrt::get_self<TimelineViewModel>(m_viewModel)->Initialize(eventQuery, strings, dispatcher);
    }

    void TimelinePage::OnTimeRangeChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        try
        {
            winrt::get_self<TimelineViewModel>(m_viewModel)->TimeRange(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
        }
    }

    void TimelinePage::OnLevelFilterChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        try
        {
            winrt::get_self<TimelineViewModel>(m_viewModel)->LevelFilter(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
        }
    }

    void TimelinePage::OnChannelFilterChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const combo = sender.as<Microsoft::UI::Xaml::Controls::ComboBox>();
        try
        {
            winrt::get_self<TimelineViewModel>(m_viewModel)->ChannelFilter(
                winrt::unbox_value<winrt::hstring>(combo.SelectedValue()));
        }
        catch (...)
        {
        }
    }

    void TimelinePage::OnRefreshClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<TimelineViewModel>(m_viewModel)->Refresh();
    }

    void TimelinePage::OnOpenClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto const item = TimelineList().SelectedItem().try_as<winrt::AstralChronicle::EventLogItemViewModel>();
        if (item) NavigateToEvent(item);
    }

    void TimelinePage::OnBookmarkClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        winrt::get_self<TimelineViewModel>(m_viewModel)->BookmarkSelected();
    }

    void TimelinePage::OnExportClicked(
        winrt::Windows::Foundation::IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ExportAsync(winrt::get_self<TimelineViewModel>(m_viewModel)->ExportText());
    }

    void TimelinePage::OnTimelineSelectionChanged(
        winrt::Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto const list = sender.as<Microsoft::UI::Xaml::Controls::ListView>();
        auto const item = list.SelectedItem().try_as<winrt::AstralChronicle::EventLogItemViewModel>();
        if (item)
        {
            winrt::get_self<TimelineViewModel>(m_viewModel)->Select(item);
        }
    }

    winrt::fire_and_forget TimelinePage::ExportAsync(winrt::hstring text)
    {
        auto lifetime = get_strong();
        co_await winrt::resume_background();
        try
        {
            auto folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto file = co_await folder.CreateFileAsync(L"Timeline-export.tsv", winrt::Windows::Storage::CreationCollisionOption::ReplaceExisting);
            co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, text);
        }
        catch (...)
        {
        }
    }

    void TimelinePage::NavigateToEvent(winrt::AstralChronicle::EventLogItemViewModel const& item)
    {
        if (!m_navigation || !item || item.RecordId().empty())
        {
            return;
        }
        auto const recordId = std::wcstoull(item.RecordId().c_str(), nullptr, 10);
        if (recordId == 0)
        {
            return;
        }
        ::AstralChronicle::navigation::NavigationRequest request;
        request.Route = L"event-logs";
        request.Channel = ::AstralChronicle::models::EventChannelIdentifier{
            std::wstring{ item.Channel().c_str() } };
        request.Query = L"*[System[EventRecordID=" + std::to_wstring(recordId) + L"]]";
        if (m_navigation->Navigate(request) && m_navigationSelectionChanged)
        {
            m_navigationSelectionChanged(request.Route);
        }
    }
}
