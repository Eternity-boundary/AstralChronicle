// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <cstdint>
#include <string>

namespace AstralChronicle::models
{
    enum class EventLevelFilter : std::uint8_t
    {
        Any,
        Critical,
        Error,
        Warning,
        Information,
    };

    struct EventFilter final
    {
        std::wstring Provider;
        std::wstring EventId;
        std::wstring Level;
        std::wstring Task;
        std::wstring Opcode;
        std::wstring Keyword;
        std::wstring ProcessId;
        std::wstring ThreadId;
        std::wstring User;
        std::wstring Computer;
        std::wstring RawXPath;
        bool AfterToday{};
        std::uint32_t AfterHours{};

        [[nodiscard]] bool HasStructuredFilter() const noexcept
        {
            return !Provider.empty() ||
                !EventId.empty() ||
                !Level.empty() ||
                !Task.empty() ||
                !Opcode.empty() ||
                !Keyword.empty() ||
                !ProcessId.empty() ||
                !ThreadId.empty() ||
                !User.empty() ||
                !Computer.empty() ||
                !RawXPath.empty() ||
                AfterToday ||
                AfterHours != 0;
        }
    };
}
