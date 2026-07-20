// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <winevt.h>

#include <utility>

namespace AstralChronicle::services
{
    class unique_evt_handle final
    {
    public:
        unique_evt_handle() noexcept = default;
        explicit unique_evt_handle(EVT_HANDLE value) noexcept : m_value(value) {}

        ~unique_evt_handle()
        {
            reset();
        }

        unique_evt_handle(unique_evt_handle const&) = delete;
        unique_evt_handle& operator=(unique_evt_handle const&) = delete;

        unique_evt_handle(unique_evt_handle&& other) noexcept
            : m_value(other.release())
        {
        }

        unique_evt_handle& operator=(unique_evt_handle&& other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
            }
            return *this;
        }

        [[nodiscard]] EVT_HANDLE get() const noexcept
        {
            return m_value;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_value != nullptr;
        }

        [[nodiscard]] EVT_HANDLE release() noexcept
        {
            return std::exchange(m_value, nullptr);
        }

        void reset(EVT_HANDLE value = nullptr) noexcept
        {
            if (m_value)
            {
                EvtClose(m_value);
            }
            m_value = value;
        }

    private:
        EVT_HANDLE m_value{};
    };
}
