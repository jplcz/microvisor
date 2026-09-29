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
        uint64_t cntvoff{0};      // Offset: CNTVCT = CNTPCT - CNTVOFF
        uint64_t cntv_cval{0};    // Compare value
        uint32_t cntv_ctl{0};     // Bit 0: Enable, Bit 1: Mask, Bit 2: Status
        uint64_t last_desched{0}; // Physical timestamp when vCPU was switched out

        // Architectural Virtualization ID registers
        uint32_t vmpidr{0};
        uint32_t vpidr{0};

        bool is_running;

        constexpr vcpu_context() noexcept
            : r{0}, sp(0), lr(0), pc(0), cpsr(0), exit_vector(0), is_running(false) {}

        void init_vcpu(uint32_t vcpu_id)
        {
            vpidr = 0x412FC0F2; // Cortex-A15 r2p2 MIDR
            // Multi-core format: Bit 31 = MP extensions, Aff0 = vcpu_id
            vmpidr = 0x80000000 | (vcpu_id & 0xFF);
        }
    };

} // namespace microvisor
