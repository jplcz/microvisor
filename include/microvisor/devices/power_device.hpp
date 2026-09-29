// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "microvisor/mmio/device.hpp"
#include <microfmt/log/macros.hpp>

namespace microvisor::devices
{

    class power_device final : public mmio::device
    {
    public:
        static constexpr uint32_t REG_STATUS = 0x00; // Offset 0x0: Read-only magic status
        static constexpr uint32_t REG_HALT = 0x04;   // Offset 0x4: Write payload to halt

        mmio::status read(uint32_t offset, uint32_t, uint32_t &val) noexcept override
        {
            if (offset == REG_STATUS)
            {
                val = 0x564D3031; // "VM01" signature
                return mmio::status::handled;
            }
            return mmio::status::unhandled;
        }

        mmio::status write(uint32_t offset, uint32_t, uint32_t val) noexcept override
        {
            if (offset == REG_HALT)
            {
                MICROFMT_LOG_INFO("[Power Device] Halt requested by guest. Payload: {:#010x}", val);
                return mmio::status::halt_vm;
            }
            return mmio::status::unhandled;
        }
    };

} // namespace microvisor::devices