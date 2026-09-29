// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "microvisor/mmio/device.hpp"
#include <microfmt/microfmt.hpp>

namespace microvisor::devices
{

    class simple_console final : public mmio::device
    {
    public:
        static constexpr uint32_t REG_DATA = 0x00;   // Offset 0x0: Read/Write Character
        static constexpr uint32_t REG_STATUS = 0x04; // Offset 0x4: Status (bit 0 = TX Ready)

        explicit simple_console(uintptr_t host_uart_base) noexcept
            : m_host_uart_base(host_uart_base) {}

        mmio::status read(uint32_t offset, uint32_t, uint32_t &val) noexcept override
        {
            if (offset == REG_STATUS)
            {
                val = 0x01; // TX always ready
                return mmio::status::handled;
            }
            val = 0;
            return mmio::status::handled;
        }

        mmio::status write(uint32_t offset, uint32_t, uint32_t val) noexcept override
        {
            if (offset == REG_DATA)
            {
                char c = static_cast<char>(val & 0xFF);
                microfmt::pl011_sink host_uart(m_host_uart_base, true);
                char buf[2] = {c, '\0'};
                host_uart.as_sink().write(buf);
                return mmio::status::handled;
            }
            return mmio::status::unhandled;
        }

    private:
        uintptr_t m_host_uart_base;
    };

} // namespace microvisor::devices