// Created by EternityBoundary on Jul 21,2026
#include "pch.h"
#include "WindowsCustomViewCatalogService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    namespace models = AstralChronicle::models;
    constexpr wchar_t CustomViewRoot[] = L"C:\\ProgramData\\Microsoft\\Event Viewer\\Views";
    constexpr wchar_t ApplicationViewsRootNode[] = L"ApplicationViewsRootNode";

    void CollectXmlFiles(
        std::wstring const& directory,
        std::vector<std::filesystem::path>& files)
    {
        auto searchPattern = directory;
        if (!searchPattern.empty() && searchPattern.back() != L'\\')
        {
            searchPattern += L'\\';
        }
        searchPattern += L'*';

        WIN32_FIND_DATAW data{};
        auto const handle = FindFirstFileW(searchPattern.c_str(), &data);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            std::wstring const name{ data.cFileName };
            if (name == L"." || name == L"..")
            {
                continue;
            }

            auto child = directory;
            if (!child.empty() && child.back() != L'\\')
            {
                child += L'\\';
            }
            child += name;

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                {
                    CollectXmlFiles(child, files);
                }
                continue;
            }

            if (std::filesystem::path{ child }.extension() == L".xml")
            {
                files.emplace_back(std::move(child));
            }
        } while (FindNextFileW(handle, &data));

        FindClose(handle);
    }

    [[nodiscard]] std::wstring ReadUtf8File(std::filesystem::path const& path)
    {
        auto const handle = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return {};
        }

        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(handle, &fileSize) ||
            fileSize.QuadPart <= 0 ||
            fileSize.QuadPart > static_cast<LONGLONG>((std::numeric_limits<int>::max)()))
        {
            CloseHandle(handle);
            return {};
        }

        std::string bytes(static_cast<std::size_t>(fileSize.QuadPart), '\0');
        DWORD bytesRead{};
        auto const read = ReadFile(
            handle,
            bytes.data(),
            static_cast<DWORD>(bytes.size()),
            &bytesRead,
            nullptr);
        CloseHandle(handle);
        if (!read || bytesRead != bytes.size())
        {
            return {};
        }

        if (bytes.empty())
        {
            return {};
        }

        auto const convert = [&bytes](UINT codePage, DWORD flags) -> std::wstring
        {
            auto const length = MultiByteToWideChar(
                codePage,
                flags,
                bytes.data(),
                static_cast<int>(bytes.size()),
                nullptr,
                0);
            if (length <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<std::size_t>(length), L'\0');
            if (MultiByteToWideChar(
                    codePage,
                    flags,
                    bytes.data(),
                    static_cast<int>(bytes.size()),
                    result.data(),
                    length) <= 0)
            {
                return {};
            }
            return result;
        };

        if (bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF)
        {
            bytes.erase(0, 3);
        }

        auto result = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
        return result.empty() ? convert(CP_ACP, 0) : result;
    }

    [[nodiscard]] std::wstring ElementText(
        std::wstring const& xml,
        std::wstring_view elementName)
    {
        auto const opening = L"<" + std::wstring{ elementName };
        auto const closing = L"</" + std::wstring{ elementName } + L">";
        auto const start = xml.find(opening);
        if (start == std::wstring::npos)
        {
            return {};
        }

        auto const contentStart = xml.find(L'>', start);
        auto const contentEnd = xml.find(closing, contentStart);
        if (contentStart == std::wstring::npos || contentEnd == std::wstring::npos)
        {
            return {};
        }
        return xml.substr(contentStart + 1, contentEnd - contentStart - 1);
    }

    [[nodiscard]] std::wstring QueryListXml(std::wstring const& xml)
    {
        auto const start = xml.find(L"<QueryList");
        auto const endMarker = L"</QueryList>";
        auto const end = xml.find(endMarker, start);
        if (start == std::wstring::npos || end == std::wstring::npos)
        {
            return {};
        }
        return xml.substr(start, end + std::wstring{ endMarker }.size() - start);
    }

    [[nodiscard]] models::CustomViewTreeNode& FindOrAdd(
        std::vector<models::CustomViewTreeNode>& siblings,
        std::wstring segment,
        std::wstring relativePath)
    {
        auto existing = std::find_if(
            siblings.begin(),
            siblings.end(),
            [&segment](models::CustomViewTreeNode const& node)
            {
                return node.Segment == segment;
            });
        if (existing != siblings.end())
        {
            return *existing;
        }

        siblings.push_back(models::CustomViewTreeNode{
            std::move(segment),
            std::move(relativePath),
            {},
            {},
            false });
        return siblings.back();
    }

    void SortTree(std::vector<models::CustomViewTreeNode>& nodes)
    {
        std::sort(
            nodes.begin(),
            nodes.end(),
            [](models::CustomViewTreeNode const& left, models::CustomViewTreeNode const& right)
            {
                if (left.IsView != right.IsView)
                {
                    return !left.IsView;
                }
                return left.Segment < right.Segment;
            });

        for (auto& node : nodes)
        {
            SortTree(node.Children);
        }
    }
}

namespace AstralChronicle::services
{
    std::vector<models::CustomViewTreeNode> WindowsCustomViewCatalogService::Enumerate() const
    {
        std::vector<models::CustomViewTreeNode> roots;
        std::filesystem::path const root{ CustomViewRoot };
        std::vector<std::filesystem::path> files;
        CollectXmlFiles(CustomViewRoot, files);
        for (auto const& filePath : files)
        {
            auto const relative = filePath.lexically_relative(root);
            if (relative.empty())
            {
                continue;
            }

            auto relativeText = relative.generic_wstring();
            if (relativeText.starts_with(std::wstring{ ApplicationViewsRootNode } + L"/"))
            {
                continue;
            }

            auto xml = ReadUtf8File(filePath);
            auto query = QueryListXml(xml);
            if (query.empty())
            {
                continue;
            }

            auto const stem = relative.stem().generic_wstring();
            auto const name = ElementText(xml, L"Name");
            auto const displayName = name.empty() ? stem : name;

            std::vector<models::CustomViewTreeNode>* siblings = &roots;
            std::wstring currentPath;
            auto const parent = relative.parent_path();
            for (auto const& segment : parent)
            {
                auto const segmentText = segment.generic_wstring();
                if (segmentText.empty() || segmentText == ApplicationViewsRootNode)
                {
                    continue;
                }
                if (!currentPath.empty())
                {
                    currentPath += L'/';
                }
                currentPath += segmentText;
                auto& folder = FindOrAdd(*siblings, segmentText, currentPath);
                siblings = &folder.Children;
            }

            auto const viewPath = relative.generic_wstring();
            auto& view = FindOrAdd(*siblings, displayName, viewPath);
            view.RelativePath = viewPath;
            view.Query = std::move(query);
            view.IsView = true;
        }

        SortTree(roots);
        return roots;
    }
}
