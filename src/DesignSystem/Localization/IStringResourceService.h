// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <winrt/Windows.Foundation.h>

namespace AstralChronicle::design
{
    struct IStringResourceService
    {
        virtual ~IStringResourceService() = default;

        [[nodiscard]] virtual winrt::hstring GetString(winrt::hstring const& resourceKey) const = 0;
    };
}
