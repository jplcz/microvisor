// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>

namespace microvisor
{

    /**
     * @brief Holds the execution context of a Virtual CPU.
     */
    struct vcpu_context
    {
        uint32_t r[13]; // 0x00 - 0x30
        uint32_t sp;    // 0x34
        uint32_t lr;    // 0x38
        uint32_t pc;    // 0x3C
        uint32_t cpsr;  // 0x40

        // New: Stores the exact hardware vector offset that caused the exit
        uint32_t exit_vector; // 0x44

        // Virtual Timer State
        uint64_t cntvoff;      // Offset: CNTVCT = CNTPCT - CNTVOFF
        uint64_t cntv_cval;    // Compare value
        uint32_t cntv_ctl;     // Bit 0: Enable, Bit 1: Mask, Bit 2: Status
        uint64_t last_desched; // Physical timestamp when vCPU was switched out

        bool is_running;

        constexpr vcpu_context() noexcept
            : r{0}, sp(0), lr(0), pc(0), cpsr(0), exit_vector(0), is_running(false) {}
    };

} // namespace microvisor
