// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <reloco/error.hpp>
#include <reloco/expected.hpp>
#include "microvisor/lpae_manager.hpp"
#include "microvisor/lpae_stage2.hpp"
#include "microvisor/vcpu.hpp"
#include <microfmt/log/macros.hpp>

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

            return m_stage2_mmu.init();
        }

        [[nodiscard]] uint8_t id() const noexcept { return m_vmid; }
        [[nodiscard]] lpae_manager &stage2() noexcept { return m_stage2_mmu; }
        [[nodiscard]] vcpu_context &vcpu() noexcept { return m_vcpu; }

        /**
         * @brief Executes the VM until a hardware exception forces an exit.
         * @return The hardware vector offset that caused the exit.
         */
        uint32_t run() noexcept
        {
            m_vcpu.is_running = true;

            // Enable Virtualization (Stage-2 MMU Routing)
            const uint32_t old_hcr = hyp_get_hcr();
            uint32_t hcr = 0;
            hcr |= (1 << 0); // VM: Enable Stage-2 translation
            hcr |= (1 << 4); // IMO: Route physical IRQs to Hyp mode
            hcr |= (1 << 3); // FMO: Route physical FIQs to Hyp mode
            hyp_set_hcr(hcr);

            // Format VTTBR (VMID + Stage-2 Root Page Table)
            uint64_t vttbr_val = (static_cast<uint64_t>(m_vmid) << 48) |
                                 (m_stage2_mmu.root_paddr().value & ~0xFFFULL);

            uint32_t vttbr_low = static_cast<uint32_t>(vttbr_val & 0xFFFFFFFF);
            uint32_t vttbr_high = static_cast<uint32_t>(vttbr_val >> 32);

            // The World Switch
            // CPU blocks here in host context, executes guest, and returns here on exit.
            hyp_enter_vm(&m_vcpu, vttbr_low, vttbr_high);

            // Disable Stage-2 routing so host memory operations aren't accidentally trapped
            hyp_set_hcr(old_hcr);

            m_vcpu.is_running = false;

            // Return the exact vector trampoline that caught the exit
            return m_vcpu.exit_vector;
        }

    private:
        uint8_t m_vmid{0};
        lpae_manager m_stage2_mmu{reloco::allocator_ref{}}; // Default to null allocator
        vcpu_context m_vcpu{};
    };

} // namespace microvisor
