// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <structo/arch/arm/hyp_vm_regs.hpp>

namespace microvisor
{

    /**
     * @brief Shadow of the guest's other-mode *banked* `SP`/`LR` (and
     * FIQ's private `R8`-`R12`), used by microvisor/sysregs.hpp to
     * save/restore this guest state across world switches between
     * different VMs.
     *
     * `hyp_common_exit` (exceptions.S) only ever saves/restores the
     * SP/LR of whichever mode was active at trap time (the plain `sp`/`lr`
     * fields of @ref vcpu_context). SP_usr/LR_usr and each of SVC/ABT/UND/
     * IRQ/FIQ's own banked SP/LR (plus FIQ's private R8-R12) are separate
     * physical registers untouched by that path, so they must be tracked
     * and switched per-VM by hand -- see microvisor/sysregs.hpp.
     *
     * The banked `SPSR`s are *not* duplicated here: they are genuine
     * system registers (unlike SP/LR, which are general-purpose), so they
     * are tracked by @ref structo::arch::arm::vm_banked_spsrs instead.
     */
    struct guest_banked_regs
    {
        uint32_t sp_usr{0};
        uint32_t lr_usr{0};

        uint32_t sp_svc{0};
        uint32_t lr_svc{0};

        uint32_t sp_abt{0};
        uint32_t lr_abt{0};

        uint32_t sp_und{0};
        uint32_t lr_und{0};

        uint32_t sp_irq{0};
        uint32_t lr_irq{0};

        uint32_t sp_fiq{0};
        uint32_t lr_fiq{0};
        uint32_t r8_fiq{0};
        uint32_t r9_fiq{0};
        uint32_t r10_fiq{0};
        uint32_t r11_fiq{0};
        uint32_t r12_fiq{0};

        // Banked SPSR_svc/abt/und/irq/fiq (see structo::arch::arm::hyp_vm_regs.hpp)
        structo::arch::arm::vm_banked_spsrs spsrs{};
    };

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

        // Guest EL1/EL0 system register shadow (see
        // structo::arch::arm::vm_guest_state in structo/arch/arm/hyp_vm_regs.hpp
        // and microvisor/sysregs.hpp). Covers both short- and LPAE-descriptor
        // TTBR0/TTBR1/PAR so a guest is free to pick either MMU descriptor
        // format for its own Stage-1 translation.
        structo::arch::arm::vm_guest_state sysregs{};

        // Guest other-mode banked register shadow (see microvisor/sysregs.hpp)
        guest_banked_regs banked{};

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
