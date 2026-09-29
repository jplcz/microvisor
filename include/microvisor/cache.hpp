// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>

namespace microvisor
{

    /**
     * @brief Bare-metal cache flush for dynamically loaded code.
     * Cleans the D-Cache and Invalidates the I-Cache for a specific memory range.
     */
    inline void clear_cache(const void *start, const void *end) noexcept
    {
        uintptr_t start_addr = reinterpret_cast<uintptr_t>(start);
        uintptr_t end_addr = reinterpret_cast<uintptr_t>(end);

        // Read Cache Type Register (CTR) to get the line sizes
        uint32_t ctr;
        asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));

        // CTR [19:16] = DminLine (Log2 of the number of 4-byte words)
        // CTR [3:0]   = IminLine (Log2 of the number of 4-byte words)
        uint32_t d_line_size = 4 << ((ctr >> 16) & 0xF);
        uint32_t i_line_size = 4 << (ctr & 0xF);

        // Clean D-Cache to Point of Unification (PoU) by MVA
        uintptr_t addr = start_addr & ~(d_line_size - 1); // Align down
        for (; addr < end_addr; addr += d_line_size)
        {
            // DCCMVAU: Data Cache Clean by MVA to PoU
            asm volatile("mcr p15, 0, %0, c7, c11, 1" ::"r"(addr) : "memory");
        }

        // Ensure D-Cache clean is fully committed before invalidating I-Cache
        asm volatile("dsb sy" ::: "memory");

        // Invalidate I-Cache to Point of Unification (PoU) by MVA
        addr = start_addr & ~(i_line_size - 1);
        for (; addr < end_addr; addr += i_line_size)
        {
            // ICIMVAU: Instruction Cache Invalidate by MVA to PoU
            asm volatile("mcr p15, 0, %0, c7, c5, 1" ::"r"(addr) : "memory");
        }

        // Final barriers to ensure instructions are flushed from the pipeline
        asm volatile(
            "dsb sy\n"
            "isb\n" ::: "memory");
    }

} // namespace microvisor