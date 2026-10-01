// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <structo/phys_addr.hpp>
#include <structo/phys_page.hpp>
#include <structo/pfn_translator.hpp>

namespace microvisor
{
    namespace lpae_stage2
    {

        // Descriptor Types (Bits 1:0)
        constexpr uint64_t TYPE_FAULT = 0b00;
        constexpr uint64_t TYPE_BLOCK = 0b01; // Valid at Level 1 (1GB) and Level 2 (2MB)
        constexpr uint64_t TYPE_TABLE = 0b11; // Valid at Level 1 and Level 2
        constexpr uint64_t TYPE_PAGE = 0b11;  // Valid at Level 3 (4KB)

        // Stage-2 Memory Attributes (MemAttr, Bits 5:2)
        // Directly encodes the cacheability instead of pointing to an MAIR register.
        constexpr uint64_t MEMATTR_DEVICE_nGnRE = 0b0100; // Device Memory
        constexpr uint64_t MEMATTR_NORMAL_WB = 0b1111;    // Normal Memory, Write-Back Write-Allocate

        // Stage-2 Access Permissions (S2AP, Bits 7:6)
        constexpr uint64_t S2AP_NONE = 0b00;
        constexpr uint64_t S2AP_RO = 0b01; // Read-Only
        constexpr uint64_t S2AP_WO = 0b10; // Write-Only
        constexpr uint64_t S2AP_RW = 0b11; // Read/Write

        // Shareability (SH, Bits 9:8)
        constexpr uint64_t SH_NON_SHAREABLE = 0b00;
        constexpr uint64_t SH_OUTER_SHAREABLE = 0b10;
        constexpr uint64_t SH_INNER_SHAREABLE = 0b11;

        /**
         * @brief 64-bit Stage-2 LPAE Descriptor Format (VM Memory Isolation)
         */
        union descriptor
        {
            uint64_t raw;

            struct
            {
                uint64_t type : 2;    // [1:0]   Descriptor Type (Table, Block, Page)
                uint64_t memattr : 4; // [5:2]   Stage-2 Memory Attributes
                uint64_t s2ap : 2;    // [7:6]   Stage-2 Access Permissions
                uint64_t sh : 2;      // [9:8]   Shareability
                uint64_t af : 1;      // [10]    Access Flag
                uint64_t ignored : 1; // [11]    Ignored / Reserved
                uint64_t paddr : 28;  // [39:12] Target Host Physical Address shifted by 12 (PFN)
                uint64_t res0_1 : 12; // [51:40] Reserved (0)
                uint64_t contig : 1;  // [52]    Contiguous hint
                uint64_t res0_2 : 1;  // [53]    Reserved (0)
                uint64_t xn : 1;      // [54]    Execute-Never
                uint64_t res0_3 : 9;  // [63:55] Reserved (0)
            } bits;

            constexpr descriptor() noexcept : raw(0) {}
            constexpr explicit descriptor(uint64_t v) noexcept : raw(v) {}

            // --------------------------------------------------------------------
            // Type-Safe Descriptor Constructors
            // --------------------------------------------------------------------

            // All Stage-2 targets must map to Host Physical RAM/Devices.
            template <typename PhysInt>
            using host_pfn_4k = structo::phys_pfn<structo::host_phys_space, structo::page_4k, PhysInt>;

            /**
             * @brief Constructs a Next-Level Table pointer.
             * Enforces that the target address belongs to the Host Physical Space.
             */
            template <typename T, typename PhysInt>
            static constexpr descriptor make_table(structo::phys_addr<T, structo::host_phys_space, PhysInt> next_level_paddr) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_TABLE;

                // from_addr securely strips the offset and computes the PFN[cite: 7, 9]
                d.bits.paddr = static_cast<uint64_t>(host_pfn_4k<PhysInt>::from_addr(next_level_paddr).value);
                return d;
            }

            /**
             * @brief Constructs a 2MB Block mapping (Level 2).
             * Enforces that the target address belongs to the Host Physical Space[cite: 8].
             */
            template <typename T, typename PhysInt>
            static constexpr descriptor make_block(structo::phys_addr<T, structo::host_phys_space, PhysInt> paddr, uint64_t memattr, uint64_t s2ap, bool xn = false) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_BLOCK;
                d.bits.memattr = memattr;
                d.bits.s2ap = s2ap;
                d.bits.sh = SH_INNER_SHAREABLE;
                d.bits.af = 1;

                d.bits.paddr = static_cast<uint64_t>(host_pfn_4k<PhysInt>::from_addr(paddr).value);
                d.bits.xn = xn ? 1 : 0;
                return d;
            }

            /**
             * @brief Constructs a terminal 4KB Page mapping (Level 3).
             * Enforces that the target address belongs to the Host Physical Space[cite: 8].
             */
            template <typename T, typename PhysInt>
            static constexpr descriptor make_page(structo::phys_addr<T, structo::host_phys_space, PhysInt> paddr, uint64_t memattr, uint64_t s2ap, bool xn = false) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_PAGE;
                d.bits.memattr = memattr;
                d.bits.s2ap = s2ap;
                d.bits.sh = SH_INNER_SHAREABLE;
                d.bits.af = 1;

                d.bits.paddr = static_cast<uint64_t>(host_pfn_4k<PhysInt>::from_addr(paddr).value);
                d.bits.xn = xn ? 1 : 0;
                return d;
            }
        };

        static_assert(sizeof(descriptor) == 8, "LPAE descriptor must be exactly 8 bytes");

    } // namespace lpae_stage2
} // namespace microvisor