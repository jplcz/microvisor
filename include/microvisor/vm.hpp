// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <reloco/error.hpp>
#include <reloco/expected.hpp>
#include "microvisor/lpae_manager.hpp"
#include "microvisor/lpae_stage2.hpp"
#include "microvisor/vcpu.hpp"
#include <microvisor/timer.hpp>
#include <microvisor/sysregs.hpp>
#include <microvisor/mmu.hpp>
#include <microfmt/log/macros.hpp>
#include <microvisor/mmio/bus.hpp>
#include <structo/arch/arm/sysregs_generated.hpp>

extern "C"
{
    // Assembly routine to execute the world switch
    void hyp_enter_vm(microvisor::vcpu_context *vcpu, uint32_t vttbr_low, uint32_t vttbr_high);
}

namespace microvisor
{

    /**
     * @brief Represents a single Virtual Machine instance.
     */
    class vm
    {
    public:
        constexpr vm() noexcept = default;

        ~vm() noexcept
        {
            microvisor::sysregs::invalidate_owner(m_vcpu);
            MICROFMT_LOG_INFO("VM destroyed");
        }

        reloco::result<void> try_construct(uint8_t vmid, reloco::allocator_ref page_alloc) noexcept
        {
            if (vmid == 0)
            {
                return reloco::unexpected(reloco::error::invalid_argument);
            }

            m_vmid = vmid;
            m_stage2_mmu = lpae_manager(page_alloc);
            m_vcpu.init_vcpu(0);

            return m_stage2_mmu.init();
        }

        [[nodiscard]] uint8_t id() const noexcept { return m_vmid; }
        [[nodiscard]] lpae_manager &stage2() noexcept { return m_stage2_mmu; }
        [[nodiscard]] vcpu_context &vcpu() noexcept { return m_vcpu; }

        /**
         * @brief Executes the VM until a hardware exception forces an exit.
         * @return The hardware vector offset that caused the exit.
         */
        uint32_t run(uint32_t slice_ms = 10) noexcept
        {
            m_vcpu.is_running = true;

            microvisor::timer::restore_guest_timer(m_vcpu);
            microvisor::sysregs::restore_guest_sysregs(m_vcpu);
            microvisor::timer::arm_preemption_timer(slice_ms);
            structo::arch::arm::sysreg_raw::vpidr{m_vcpu.vpidr}.write();
            structo::arch::arm::sysreg_raw::vmpidr{m_vcpu.vmpidr}.write();

            // Enable Virtualization (Stage-2 MMU Routing)
            const auto old_hcr = structo::arch::arm::sysreg_raw::hcr::read();
            structo::arch::arm::sysreg_raw::hcr hcr{};
            hcr.set_vm(true)   // VM: Enable Stage-2 translation
                .set_imo(true) // IMO: Route physical IRQs to Hyp mode
                .set_fmo(true); // FMO: Route physical FIQs to Hyp mode
            hcr.write();

            // Format VTTBR (VMID + Stage-2 Root Page Table)
            structo::arch::arm::sysreg_raw::vttbr vttbr{};
            vttbr.set_vmid(m_vmid);
            vttbr.raw |= (m_stage2_mmu.root_paddr().value & ~0xFFFULL);

            uint32_t vttbr_low = static_cast<uint32_t>(vttbr.raw & 0xFFFFFFFF);
            uint32_t vttbr_high = static_cast<uint32_t>(vttbr.raw >> 32);

            MICROFMT_LOG_INFO("VTTBR {:#x}", vttbr.raw);

            // The World Switch
            // CPU blocks here in host context, executes guest, and returns here on exit.
            hyp_enter_vm(&m_vcpu, vttbr_low, vttbr_high);

            // Disable Stage-2 routing so host memory operations aren't accidentally trapped
            old_hcr.write();
            microvisor::timer::disarm_preemption_timer();
            microvisor::timer::save_guest_timer(m_vcpu);
            microvisor::sysregs::save_guest_sysregs(m_vcpu);

            m_vcpu.is_running = false;

            // Return the exact vector trampoline that caught the exit
            return m_vcpu.exit_vector;
        }

        [[nodiscard]] mmio::bus<16> &mmio() noexcept { return m_mmio_bus; }
        [[nodiscard]] const mmio::bus<16> &mmio() const noexcept { return m_mmio_bus; }

    private:
        uint8_t m_vmid{0};
        lpae_manager m_stage2_mmu{reloco::allocator_ref{}}; // Default to null allocator
        vcpu_context m_vcpu{};
        mmio::bus<16> m_mmio_bus;
    };

} // namespace microvisor
