// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "DesignSystem/Localization/StringResourceService.h"

#include <string>

using namespace winrt;
using namespace Microsoft::Windows::ApplicationModel::Resources;

namespace AstralChronicle::design
{
    StringResourceService::StringResourceService()
    {
        try
        {
            m_loader = ResourceLoader{};
        }
        catch (...)
        {
            // Keep the service usable in design-time or unpackaged contexts.
        }
    }

    hstring StringResourceService::GetString(hstring const& resourceKey) const
    {
        try
        {
            if (m_loader)
            {
                // MRT resource names with segments use '/' when loaded from
                // code, while their .resw identifiers retain '.' separators.
                auto lookupKey = std::wstring{ resourceKey.c_str() };
                for (auto& character : lookupKey)
                {
                    if (character == L'.')
                    {
                        character = L'/';
                    }
                }

                auto const value = m_loader.GetString(hstring{ lookupKey });
                if (!value.empty())
                {
                    return value;
                }
            }
        }
        catch (...)
        {
            // Returning the key makes a missing translation diagnosable while
            // keeping the UI usable instead of throwing during page creation.
        }

        return resourceKey;
    }
}
