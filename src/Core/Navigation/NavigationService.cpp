// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "NavigationService.h"

#include <stdexcept>

namespace AstralChronicle::navigation
{
    void NavigationService::Attach(winrt::Microsoft::UI::Xaml::Controls::Frame const& frame)
    {
        if (!frame)
        {
            throw std::invalid_argument("Navigation requires a content frame.");
        }

        m_frame = frame;
    }

    void NavigationService::Register(PageRegistration registration)
    {
        if (registration.Route.empty() ||
            (!registration.CreatePage && !registration.CreatePageWithRequest))
        {
            throw std::invalid_argument("A page registration needs a route and a factory.");
        }

        m_factories.insert_or_assign(registration.Route, std::move(registration));
    }

    bool NavigationService::Navigate(std::wstring_view route)
    {
        return Navigate(NavigationRequest{ std::wstring{ route }, std::nullopt });
    }

    bool NavigationService::Navigate(NavigationRequest const& request)
    {
        if (!m_frame)
        {
            throw std::logic_error("Attach a content frame before navigating.");
        }

        auto const page = m_factories.find(request.Route);
        if (page == m_factories.end())
        {
            return false;
        }

        // Factories create a page only when its route is selected, which keeps feature pages lazy.
        if (page->second.CreatePageWithRequest)
        {
            m_frame.Content(page->second.CreatePageWithRequest(request));
        }
        else if (page->second.CreatePage)
        {
            m_frame.Content(page->second.CreatePage());
        }
        else
        {
            return false;
        }

        return true;
    }
}
