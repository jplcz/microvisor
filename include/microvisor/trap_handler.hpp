// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <cstddef>
#include <microfmt/microfmt.hpp>
#include <microfmt/log/macros.hpp>
#include "microvisor/vm.hpp"

extern "C"
{
    uint32_t hyp_get_hsr();
    uint32_t hyp_get_hdfar();
    uint32_t hyp_get_hifar();
    uint32_t hyp_get_hpfar();
}

namespace microvisor::trap
{

    // ------------------------------------------------------------------------
    // Exception Classes (HSR Bits [31:26])
    // ------------------------------------------------------------------------
    namespace ec
    {
        constexpr uint32_t UNKNOWN = 0x00;
        constexpr uint32_t WFI_WFE = 0x01;
        constexpr uint32_t CP15_32 = 0x03; // MRC / MCR
        constexpr uint32_t CP15_64 = 0x04; // MRRC / MCRR
        constexpr uint32_t CP14_32 = 0x05;
        constexpr uint32_t CP14_64 = 0x06;
        constexpr uint32_t HCPTR_TRAP = 0x07; // VFP / SIMD trap
        constexpr uint32_t HVC = 0x12;
        constexpr uint32_t SMC = 0x13;
        constexpr uint32_t PREFETCH_ABT_GUEST = 0x20; // Stage-2 Prefetch Abort
        constexpr uint32_t PREFETCH_ABT_HOST = 0x21;  // Host EL2 Prefetch Abort (Panic)
        constexpr uint32_t DATA_ABT_GUEST = 0x24;     // Stage-2 Data Abort (MMIO)
        constexpr uint32_t DATA_ABT_HOST = 0x25;      // Host EL2 Data Abort (Panic)
    }

    enum class exit_status
    {
        resume, // Re-enter guest execution
        halt    // Stop VM run loop
    };

    // ------------------------------------------------------------------------
    // Strongly-Typed HSR Syndrome Parser
    // ------------------------------------------------------------------------
    struct hsr_syndrome
    {
        uint32_t raw;

        constexpr explicit hsr_syndrome(uint32_t val) noexcept : raw(val) {}

        [[nodiscard]] constexpr uint32_t exception_class() const noexcept { return (raw >> 26) & 0x3F; }
        [[nodiscard]] constexpr bool il() const noexcept { return (raw & (1 << 25)) != 0; }
        [[nodiscard]] constexpr uint32_t iss() const noexcept { return raw & 0x01FFFFFF; }

        // Data Abort Specific Syndrome fields
        [[nodiscard]] constexpr bool isv() const noexcept { return (raw & (1 << 24)) != 0; }
        [[nodiscard]] constexpr uint32_t sas() const noexcept { return (raw >> 22) & 0x3; }  // 0: 8-bit, 1: 16-bit, 2: 32-bit
        [[nodiscard]] constexpr bool sse() const noexcept { return (raw & (1 << 21)) != 0; } // Sign extended
        [[nodiscard]] constexpr uint32_t srt() const noexcept { return (raw >> 16) & 0xF; }  // Register index
        [[nodiscard]] constexpr bool is_write() const noexcept { return (raw & (1 << 6)) != 0; }
        [[nodiscard]] constexpr uint32_t dfsc() const noexcept { return raw & 0x3F; }

        [[nodiscard]] constexpr uint32_t access_size() const noexcept
        {
            return 1u << sas();
        }
    };

    // ------------------------------------------------------------------------
    // VCPU Register Access Helpers
    // ------------------------------------------------------------------------
    inline uint32_t get_vcpu_reg(const vcpu_context &vcpu, uint32_t reg) noexcept
    {
        if (reg < 13)
            return vcpu.r[reg];
        if (reg == 13)
            return vcpu.sp;
        if (reg == 14)
            return vcpu.lr;
        return vcpu.pc;
    }

    inline void set_vcpu_reg(vcpu_context &vcpu, uint32_t reg, uint32_t value) noexcept
    {
        if (reg < 13)
            vcpu.r[reg] = value;
        else if (reg == 13)
            vcpu.sp = value;
        else if (reg == 14)
            vcpu.lr = value;
        else
            vcpu.pc = value;
    }

    inline void advance_pc(vcpu_context &vcpu, hsr_syndrome hsr) noexcept
    {
        vcpu.pc += hsr.il() ? 4 : 2;
    }

    inline uint32_t get_fault_ipa() noexcept
    {
        uint32_t hpfar = hyp_get_hpfar();
        uint32_t hdfar = hyp_get_hdfar();
        if (hpfar != 0)
        {
            // HPFAR holds IPA[39:12] in bits [31:4]. Low 12 bits come from VA (HDFAR).
            return ((hpfar & ~0xFu) << 8) | (hdfar & 0x0FFFu);
        }
        return hdfar;
    }

    // ------------------------------------------------------------------------
    // Structured MMIO Subsystem
    // ------------------------------------------------------------------------
    struct mmio_access
    {
        uint32_t ipa;
        uint32_t value;
        uint32_t size; // 1, 2, or 4 bytes
        bool is_write;
    };

    inline exit_status emulate_mmio(vm &current_vm, mmio_access &access) noexcept
    {
        // Device 1: Virtual Power / Control Register (0x80000000)
        if (access.ipa == 0x80000000)
        {
            if (access.is_write)
            {
                MICROFMT_LOG_INFO("[VM {}] MMIO Power Event. Payload: {:#010x}", current_vm.id(), access.value);
                return exit_status::halt;
            }
            else
            {
                access.value = 0x564D3031; // "VM01" signature
                return exit_status::resume;
            }
        }

        // Device 2: Virtual Console / Putchar (0x80000004)
        if (access.ipa == 0x80000004)
        {
            if (access.is_write)
            {
                char c = static_cast<char>(access.value & 0xFF);
                microfmt::pl011_sink uart(0x09000000, true);
                char buf[2] = {c, '\0'};
                uart.as_sink().write(buf);
            }
            else
            {
                access.value = 0x00; // TX Ready flag
            }
            return exit_status::resume;
        }

        MICROFMT_LOG_ERROR("[VM {}] Unhandled MMIO {} at IPA {:#010x}, Size: {} bytes",
                           current_vm.id(),
                           access.is_write ? "WRITE" : "READ",
                           access.ipa,
                           access.size);
        return exit_status::halt;
    }

    // ------------------------------------------------------------------------
    // Trap Handlers
    // ------------------------------------------------------------------------
    inline exit_status handle_data_abort(vm &current_vm, hsr_syndrome hsr) noexcept
    {
        if (!hsr.isv())
        {
            MICROFMT_LOG_ERROR("[VM {}] Data abort with ISV=0 at PC {:#010x}",
                               current_vm.id(), current_vm.vcpu().pc);
            return exit_status::halt;
        }

        uint32_t ipa = get_fault_ipa();
        uint32_t srt = hsr.srt();
        uint32_t size = hsr.access_size();
        bool is_write = hsr.is_write();

        uint32_t val = 0;
        if (is_write)
        {
            val = get_vcpu_reg(current_vm.vcpu(), srt);
            if (size == 1)
                val &= 0xFF;
            else if (size == 2)
                val &= 0xFFFF;
        }

        // Dispatch via the VM's MMIO router
        mmio::status mmio_res = current_vm.mmio().dispatch(ipa, size, is_write, val);

        switch (mmio_res)
        {
        case mmio::status::handled:
            if (!is_write)
            {
                // Apply sign extension if requested by hardware
                if (hsr.sse())
                {
                    if (size == 1)
                        val = static_cast<uint32_t>(static_cast<int8_t>(val));
                    else if (size == 2)
                        val = static_cast<uint32_t>(static_cast<int16_t>(val));
                }
                set_vcpu_reg(current_vm.vcpu(), srt, val);
            }
            advance_pc(current_vm.vcpu(), hsr);
            return exit_status::resume;

        case mmio::status::halt_vm:
            // Intentional shutdown: step PC and stop run loop
            advance_pc(current_vm.vcpu(), hsr);
            return exit_status::halt;

        case mmio::status::unhandled:
        default:
            MICROFMT_LOG_ERROR("[VM {}] Unhandled MMIO {} at IPA {:#010x} (PC: {:#010x})",
                               current_vm.id(),
                               is_write ? "WRITE" : "READ",
                               ipa,
                               current_vm.vcpu().pc);
            return exit_status::halt;
        }
    }
    inline exit_status handle_hvc(vm &current_vm, hsr_syndrome hsr) noexcept
    {
        uint32_t imm16 = hsr.iss() & 0xFFFF;
        MICROFMT_LOG_INFO("[VM {}] HVC call {:#06x}", current_vm.id(), imm16);

        switch (imm16)
        {
        case 0x00: // Halt
            return exit_status::halt;

        case 0x100:
        { // Putchar via r0
            char c = static_cast<char>(current_vm.vcpu().r[0] & 0xFF);
            microfmt::pl011_sink uart(0x09000000, true);
            char buf[2] = {c, '\0'};
            uart.as_sink().write(buf);
            return exit_status::resume;
        }

        default:
            MICROFMT_LOG_WARN("[VM {}] Unknown HVC #{:#06x}", current_vm.id(), imm16);
            return exit_status::resume;
        }
    }

    inline exit_status handle_cp15(vm &current_vm, hsr_syndrome hsr) noexcept
    {
        uint32_t iss = hsr.iss();
        bool is_read = (iss & 1) != 0;
        uint32_t crm = (iss >> 1) & 0xF;
        uint32_t rt = (iss >> 5) & 0xF;
        uint32_t crn = (iss >> 10) & 0xF;
        uint32_t opc1 = (iss >> 14) & 0x7;
        uint32_t opc2 = (iss >> 17) & 0x7;

        MICROFMT_LOG_DEBUG("[VM {}] Trapped CP15 {} p15, {}, r{}, c{}, c{}, {}",
                           current_vm.id(), is_read ? "MRC" : "MCR", opc1, rt, crn, crm, opc2);

        // Minimal dummy emulation: reads return 0, writes are ignored
        if (is_read && rt != 15)
        {
            set_vcpu_reg(current_vm.vcpu(), rt, 0);
        }

        advance_pc(current_vm.vcpu(), hsr);
        return exit_status::resume;
    }

    // ------------------------------------------------------------------------
    // Master VM Exit Router
    // ------------------------------------------------------------------------
    inline exit_status dispatch_exit(vm &current_vm) noexcept
    {
        // Step 1: Check trap index
        switch (current_vm.vcpu().exit_vector)
        {
        case 0x18: // IRQ
            uint32_t cnthp_ctl = timer_get_cnthp_ctl();
            if ((cnthp_ctl & timer::CTL_ISTATUS) != 0)
            {
                // Preemption timer expired
                MICROFMT_LOG_DEBUG("[VM {}] Preemption slice expired via CNTHP.", current_vm.id());

                // Disarm so it stops asserting
                timer::disarm_preemption_timer();

                // Return resume to continue or trigger scheduler loop
                return exit_status::resume;
            }

            // Other platform IRQ (e.g. UART, VirtIO device)
            return exit_status::resume;
        }

        // Step 2: Parse Synchronous Trap via HSR
        hsr_syndrome hsr(hyp_get_hsr());
        uint32_t ec_val = hsr.exception_class();

        switch (ec_val)
        {
        case ec::DATA_ABT_GUEST:
            return handle_data_abort(current_vm, hsr);

        case ec::HVC:
            return handle_hvc(current_vm, hsr);

        case ec::CP15_32:
            return handle_cp15(current_vm, hsr);

        case ec::WFI_WFE:
            // Advance past WFI and treat as a guest yield
            advance_pc(current_vm.vcpu(), hsr);
            return exit_status::resume;

        case ec::PREFETCH_ABT_GUEST:
            MICROFMT_LOG_ERROR("[VM {}] Stage-2 Instruction Fetch Abort at IPA: {:#010x}",
                               current_vm.id(), hyp_get_hifar());
            return exit_status::halt;

        // Host / EL2 Fatal Panics
        case ec::DATA_ABT_HOST:
            MICROFMT_LOG_ERROR("FATAL: Hypervisor Host Data Abort at {:#010x}! PC: {:#010x}",
                               hyp_get_hdfar(), current_vm.vcpu().pc);
            return exit_status::halt;

        case ec::PREFETCH_ABT_HOST:
            MICROFMT_LOG_ERROR("FATAL: Hypervisor Host Prefetch Abort at {:#010x}!", hyp_get_hifar());
            return exit_status::halt;

        default:
            MICROFMT_LOG_ERROR("[VM {}] Unhandled Trap: EC={:#04x}, HSR={:#010x}, PC={:#010x}",
                               current_vm.id(), ec_val, hsr.raw, current_vm.vcpu().pc);
            return exit_status::halt;
        }
    }

} // namespace microvisor::trap