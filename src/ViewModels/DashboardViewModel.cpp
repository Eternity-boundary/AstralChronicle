// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DashboardViewModel.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "Services/IEventLogCatalogService.h"
#include "Services/IEventQueryService.h"

#include "DashboardViewModel.g.cpp"

#include <string>
#include <vector>

namespace winrt::AstralChronicle::implementation
{
    namespace
    {
        winrt::hstring FormatResource(
            winrt::hstring format,
            std::vector<winrt::hstring> const& values)
        {
            auto result = std::wstring{ format.c_str() };
            for (std::size_t index = 0; index < values.size(); ++index)
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
    }

    DashboardViewModel::DashboardViewModel()
    {
    }

    void DashboardViewModel::Initialize(
        ::AstralChronicle::services::IEventLogCatalogService const& eventLogCatalog,
        ::AstralChronicle::services::IEventQueryService const& eventQuery,
        ::AstralChronicle::design::IStringResourceService const& strings)
    {
        m_heading = strings.GetString(L"Dashboard.Heading");
        m_summary = strings.GetString(L"Dashboard.Summary.Initial");
        m_channelCountLabel = strings.GetString(L"Dashboard.ChannelCountLoading");
        m_recentEventPreview = strings.GetString(L"Dashboard.RecentEventLoading");

        auto const channelCount = eventLogCatalog.GetAvailableChannelCount();
        m_summary = strings.GetString(L"Dashboard.Summary.Ready");
        m_channelCountLabel = FormatResource(
            strings.GetString(L"Dashboard.ChannelCountLabel"),
            { winrt::to_hstring(channelCount) });

        auto const events = eventQuery.QueryRecent(L"System", 1);
        if (!events.empty())
        {
            auto const& latest = events.front();
            m_recentEventPreview = FormatResource(
                strings.GetString(L"Dashboard.RecentEventTemplate"),
                {
                    winrt::hstring{ latest.Provider },
                    winrt::to_hstring(latest.EventId),
                    winrt::to_hstring(latest.RecordId)
                });
        }
        else
        {
            m_recentEventPreview = strings.GetString(L"Dashboard.NoSystemEvents");
        }

        RaisePropertyChanged(L"Summary");
        RaisePropertyChanged(L"ChannelCountLabel");
        RaisePropertyChanged(L"RecentEventPreview");
    }

    winrt::hstring DashboardViewModel::Heading() const
    {
        return m_heading;
    }

    winrt::hstring DashboardViewModel::Summary() const
    {
        return m_summary;
    }

    winrt::hstring DashboardViewModel::ChannelCountLabel() const
    {
        return m_channelCountLabel;
    }

    winrt::hstring DashboardViewModel::RecentEventPreview() const
    {
        return m_recentEventPreview;
    }

    winrt::event_token DashboardViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void DashboardViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void DashboardViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
