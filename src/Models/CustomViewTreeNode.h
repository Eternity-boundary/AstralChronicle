// Created by EternityBoundary on Jul 21,2026
#pragma once

#include <string>
#include <vector>

namespace AstralChronicle::models
{
    struct CustomViewTreeNode final
    {
        std::wstring Segment;
        std::wstring RelativePath;
        std::wstring Query;
        std::vector<CustomViewTreeNode> Children;
        bool IsView{};
    };
}
