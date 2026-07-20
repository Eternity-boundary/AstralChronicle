// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AstralChronicle::models
{
    enum class SavedViewType : std::uint32_t
    {
        Pinned,
        WindowsCustomView,
        User,
        Bookmark,
        FavoriteLog,
        RecentSearch,
        Workspace
    };

    struct SavedView final
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring Description;
        std::wstring Query{ L"*" };
        std::wstring Channel{ L"System" };
        SavedViewType Type{ SavedViewType::User };
        std::wstring Sort{ L"TimeCreated" };
        std::wstring Columns{ L"Level,TimeCreated,Provider,EventId,Task,Channel,User,Computer,Description" };
        bool Details{};
        bool Timeline{};
        std::wstring Tags;
        std::wstring CreatedAt;
        std::wstring UpdatedAt;
        bool IsPinned{};
        std::wstring CustomViewXml;
    };
}
