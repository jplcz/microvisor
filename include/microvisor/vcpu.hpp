// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>

namespace microvisor
{

    /**
     * @brief Shadow copy of the guest's (non-secure PL1/EL1) architectural
     * system registers, used by microvisor/sysregs.hpp to save/restore
     * guest MMU/exception-vector/thread-ID state across world switches
     * between different VMs. See sysregs.hpp for the save/restore policy;
     * this struct is just the storage.
     */
    struct guest_sysregs
    {
        uint32_t sctlr{0};
        uint32_t actlr{0};
        uint32_t cpacr{0};
        uint32_t ttbr0{0};
        uint32_t ttbr1{0};
        uint32_t ttbcr{0};
        uint32_t dacr{0};
        uint32_t mair0{0};
        uint32_t mair1{0};
        uint32_t amair0{0};
        uint32_t amair1{0};
        uint32_t vbar{0};
        uint32_t contextidr{0};
        uint32_t tpidrurw{0};
        uint32_t tpidruro{0};
        uint32_t tpidrprw{0};
    };

    /**
     * @brief Shadow of the guest's other-mode banked registers.
     *
     * `hyp_common_exit` (exceptions.S) only ever saves/restores the
     * SP/LR of whichever mode was active at trap time (the plain `sp`/`lr`
     * fields above). SP_usr/LR_usr and each of SVC/ABT/UND/IRQ/FIQ's own
     * banked SP/LR/SPSR (plus FIQ's private R8-R12) are separate physical
     * registers untouched by that path, so -- exactly like @ref
     * guest_sysregs -- they must be tracked and switched per-VM by hand;
     * see microvisor/sysregs.hpp.
     */
    struct guest_banked_regs
    {
        uint32_t sp_usr{0};
        uint32_t lr_usr{0};

        uint32_t sp_svc{0};
        uint32_t lr_svc{0};
        uint32_t spsr_svc{0};

        uint32_t sp_abt{0};
        uint32_t lr_abt{0};
        uint32_t spsr_abt{0};

        uint32_t sp_und{0};
        uint32_t lr_und{0};
        uint32_t spsr_und{0};

        uint32_t sp_irq{0};
        uint32_t lr_irq{0};
        uint32_t spsr_irq{0};

        uint32_t sp_fiq{0};
        uint32_t lr_fiq{0};
        uint32_t spsr_fiq{0};
        uint32_t r8_fiq{0};
        uint32_t r9_fiq{0};
        uint32_t r10_fiq{0};
        uint32_t r11_fiq{0};
        uint32_t r12_fiq{0};
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

        // Guest EL1 system register shadow (see microvisor/sysregs.hpp)
        guest_sysregs sysregs{};

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
