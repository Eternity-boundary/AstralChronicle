// Created by EternityBoundary on Jul 20,2026
#include "ChannelPathGrouping.h"

#include <algorithm>
#include <string_view>

namespace AstralChronicle::services
{
    namespace
    {
        [[nodiscard]] std::vector<std::wstring> SplitPath(std::wstring_view path)
        {
            std::vector<std::wstring> segments;
            std::size_t segmentStart{};

            while (segmentStart < path.size())
            {
                auto const separator = path.find_first_of(L"\\/", segmentStart);
                auto const segmentEnd = separator == std::wstring_view::npos ? path.size() : separator;
                if (segmentEnd > segmentStart)
                {
                    segments.emplace_back(path.substr(segmentStart, segmentEnd - segmentStart));
                }

                if (separator == std::wstring_view::npos)
                {
                    break;
                }

                segmentStart = separator + 1;
            }

            return segments;
        }

        ChannelPathTreeNode& FindOrAddNode(
            std::vector<ChannelPathTreeNode>& siblings,
            std::wstring const& segment,
            std::wstring const& fullPath)
        {
            auto const existing = std::find_if(
                siblings.begin(),
                siblings.end(),
                [&segment](ChannelPathTreeNode const& node)
                {
                    return node.Segment == segment;
                });
            if (existing != siblings.end())
            {
                return *existing;
            }

            siblings.push_back(ChannelPathTreeNode{ segment, fullPath, std::nullopt, {} });
            return siblings.back();
        }

        void SortTree(std::vector<ChannelPathTreeNode>& nodes)
        {
            std::sort(
                nodes.begin(),
                nodes.end(),
                [](ChannelPathTreeNode const& left, ChannelPathTreeNode const& right)
                {
                    return left.Segment < right.Segment;
                });

            for (auto& node : nodes)
            {
                SortTree(node.Children);
            }
        }
    }

    std::vector<ChannelPathTreeNode> BuildChannelPathTree(
        std::vector<models::EventChannelDescriptor> const& channels)
    {
        std::vector<ChannelPathTreeNode> roots;

        for (auto const& channel : channels)
        {
            auto const segments = SplitPath(channel.Path);
            if (segments.empty())
            {
                continue;
            }

            std::vector<ChannelPathTreeNode>* siblings = &roots;
            std::wstring currentPath;
            ChannelPathTreeNode* leaf{};

            for (auto const& segment : segments)
            {
                if (!currentPath.empty())
                {
                    currentPath += L'\\';
                }
                currentPath += segment;

                auto& node = FindOrAddNode(*siblings, segment, currentPath);
                leaf = &node;
                siblings = &node.Children;
            }

            if (leaf && !leaf->Channel)
            {
                leaf->Channel = channel;
            }
        }

        SortTree(roots);
        return roots;
    }
}
