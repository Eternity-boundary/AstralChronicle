// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "DesignSystem/Localization/IStringResourceService.h"

#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

namespace AstralChronicle::design
{
    class StringResourceService final : public IStringResourceService
    {
    public:
        StringResourceService();

        [[nodiscard]] winrt::hstring GetString(winrt::hstring const& resourceKey) const override;

    private:
        winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader m_loader{ nullptr };
    };
}
