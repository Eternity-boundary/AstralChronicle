// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "Models/EventFilter.h"

#include <string>

namespace AstralChronicle::services
{
    [[nodiscard]] std::wstring BuildEventQuery(
        models::EventFilter const& filter);
}
