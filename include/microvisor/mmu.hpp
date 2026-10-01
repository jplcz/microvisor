// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "microvisor/lpae_manager.hpp"
#include "microvisor/lpae_stage1.hpp"
#include "microvisor/memory_map.hpp"
#include <reloco/optional.hpp>
#include <structo/phys_addr.hpp>

extern "C"
{
    void hyp_set_htcr(uint32_t val);
    void hyp_set_httbr(uint32_t low, uint32_t high);
    void hyp_set_hmair0(uint32_t val);
    uint32_t hyp_get_hsctlr();
    void hyp_set_hsctlr(uint32_t val);
    uint32_t hyp_get_hcr();
    void hyp_set_hcr(uint32_t val);

    void hyp_set_vpidr(uint32_t val);
    void hyp_set_vmpidr(uint32_t val);

    // Invalidate entire Non-secure TLB (Stage-1 and Stage-2, all VMIDs)
    void hyp_tlbi_all_nsnh();
}

namespace microvisor
{

    // Global MMU Manager instance for EL2
    extern reloco::optional<lpae_manager> g_stage1_mmu;

    inline void enable_hypervisor_mmu(const memory_map &regions) noexcept
    {
        // Configure Memory Attributes (HMAIR0)
        // Index 1: Normal Memory (WBWA), Index 0: Device Memory (nGnRE)
        uint32_t hmair0 = (0xFF << 8) | 0x04;
        hyp_set_hmair0(hmair0);

        // Configure Translation Control (HTCR)
        // T0SZ=0 (4GB), IRGN0=01 (WBWA), ORGN0=01 (WBWA), SH0=11 (Inner Shareable)
        uint32_t htcr = (0 << 0) | (1 << 8) | (1 << 10) | (3 << 12);
        hyp_set_htcr(htcr);

        // Define our strongly-typed physical address for the host[cite: 8]
        using host_addr_t = structo::phys_addr<void, structo::host_phys_space, uint64_t>;

        // Map the PL011 UART (MMIO, 4KB Page, Execute Never)
        host_addr_t uart_base{0x09000000};
        uint64_t uart_desc = lpae_stage1::descriptor::make_page(
                                 uart_base,
                                 lpae_stage1::ATTR_INDEX_DEVICE,
                                 lpae_stage1::AP_RW,
                                 true // Execute Never (XN)
                                 )
                                 .raw;
        g_stage1_mmu->map_page_4k(uart_base, uart_desc).unwrap();

        // Map every region of actual installed RAM (as discovered from the
        // devicetree by `bootstrap_memory_regions`) using 2MB Blocks, instead
        // of assuming a hardcoded 128MB @ 0x40000000 layout. This provides
        // the hypervisor 1:1 access to all memory (for VM allocation) with
        // zero TLB pressure.
        constexpr uint32_t block_size = 2 * 1024 * 1024;
        regions.total.iter().for_each(
            [&](const auto &region)
            {
                // Block descriptors require 2MB-aligned addresses, so round
                // the region's bounds out to the nearest enclosing blocks.
                uint32_t base = region.base & ~(block_size - 1);
                uint32_t end = (region.end() + block_size - 1) & ~(block_size - 1);

                for (uint32_t offset = base; offset < end; offset += block_size)
                {
                    host_addr_t ram_block{offset};

                    uint64_t ram_desc = lpae_stage1::descriptor::make_block(
                                            ram_block,
                                            lpae_stage1::ATTR_INDEX_NORMAL,
                                            lpae_stage1::AP_RW,
                                            false // Allow Execution (hypervisor text lives here)
                                            )
                                            .raw;

                    g_stage1_mmu->map_block_2m(ram_block, ram_desc).unwrap();
                }
            });

        // 6. Activate MMU & Caches
        // The root_paddr() explicitly returns a typed table_addr_t[cite: 8], preventing mix-ups.
        hyp_set_httbr(g_stage1_mmu->root_paddr().value, 0);

        uint32_t hsctlr = hyp_get_hsctlr();
        hsctlr |= (1 << 0);  // M: MMU enable
        hsctlr |= (1 << 2);  // C: Data cache enable
        hsctlr |= (1 << 12); // I: Instruction cache enable
        hyp_set_hsctlr(hsctlr);
    }

} // namespace microvisor