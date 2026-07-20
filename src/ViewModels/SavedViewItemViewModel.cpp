// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SavedViewItemViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "SavedViewItemViewModel.g.cpp"

namespace
{
    [[nodiscard]] winrt::hstring TypeResourceKey(::AstralChronicle::models::SavedViewType const type)
    {
        using Type = ::AstralChronicle::models::SavedViewType;
        switch (type)
        {
        case Type::Pinned: return L"SavedViews.Type.Pinned";
        case Type::WindowsCustomView: return L"SavedViews.Type.WindowsCustomView";
        case Type::Bookmark: return L"SavedViews.Type.Bookmark";
        case Type::FavoriteLog: return L"SavedViews.Type.FavoriteLog";
        case Type::RecentSearch: return L"SavedViews.Type.RecentSearch";
        case Type::Workspace: return L"SavedViews.Type.Workspace";
        case Type::User:
        default: return L"SavedViews.Type.User";
        }
    }
}

namespace winrt::AstralChronicle::implementation
{
    void SavedViewItemViewModel::Initialize(
        ::AstralChronicle::models::SavedView const& view,
        ::AstralChronicle::design::IStringResourceService const& strings)
    {
        m_id = view.Id;
        m_name = view.Name;
        m_description = view.Description;
        m_type = strings.GetString(TypeResourceKey(view.Type));
        m_channel = view.Channel;
        m_query = view.Query;
        m_updatedAt = view.UpdatedAt;
        m_isPinned = view.IsPinned;
    }

    winrt::hstring SavedViewItemViewModel::Id() const { return m_id; }
    winrt::hstring SavedViewItemViewModel::Name() const { return m_name; }
    winrt::hstring SavedViewItemViewModel::Description() const { return m_description; }
    winrt::hstring SavedViewItemViewModel::Type() const { return m_type; }
    winrt::hstring SavedViewItemViewModel::Channel() const { return m_channel; }
    winrt::hstring SavedViewItemViewModel::Query() const { return m_query; }
    winrt::hstring SavedViewItemViewModel::UpdatedAt() const { return m_updatedAt; }
    bool SavedViewItemViewModel::IsPinned() const noexcept { return m_isPinned; }
    void SavedViewItemViewModel::IsPinned(bool const value)
    {
        if (m_isPinned != value)
        {
            m_isPinned = value;
            m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsPinned" });
        }
    }

    winrt::event_token SavedViewItemViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void SavedViewItemViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }
}
