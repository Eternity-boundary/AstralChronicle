// Created by EternityBoundary on Jul 27,2026
#pragma once

#include "EventRecordSummary.h"

#include <string>

namespace AstralChronicle::models
{
    // The transport-neutral payload for one event received by a live subscription.
    // It intentionally contains no WinRT or UI types, so services can prepare it
    // before the ViewModel is notified.
    struct LiveEventRecord final
    {
        EventRecordSummary Summary;
        std::wstring RawXml;
    };
}
