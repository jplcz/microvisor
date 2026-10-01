// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <structo/phys_addr.hpp>
#include <structo/phys_page.hpp>
#include <structo/pfn_translator.hpp>

namespace microvisor
{
    namespace lpae_stage1
    {

        // Descriptor Types (Bits 1:0)
        constexpr uint64_t TYPE_FAULT = 0b00;
        constexpr uint64_t TYPE_BLOCK = 0b01; // Valid at Level 1 and 2
        constexpr uint64_t TYPE_TABLE = 0b11; // Valid at Level 1 and 2
        constexpr uint64_t TYPE_PAGE = 0b11;  // Valid at Level 3

        // Indices into HMAIR0
        constexpr uint64_t ATTR_INDEX_DEVICE = 0;
        constexpr uint64_t ATTR_INDEX_NORMAL = 1;

        // Stage-1 Access Permissions for EL2 (AP[2:1])
        constexpr uint64_t AP_RW = 0b00; // Read/Write
        constexpr uint64_t AP_RO = 0b10; // Read-Only

        // Shareability
        constexpr uint64_t SH_INNER_SHAREABLE = 0b11;

        /**
         * @brief 64-bit Stage-1 LPAE Descriptor Format (EL2)
         */
        union descriptor
        {
            uint64_t raw;

            struct
            {
                uint64_t type : 2;    // [1:0]   Descriptor Type
                uint64_t attridx : 3; // [4:2]   Index into HMAIR0
                uint64_t ns : 1;      // [5]     Non-Secure bit
                uint64_t ap : 2;      // [7:6]   Access Permissions
                uint64_t sh : 2;      // [9:8]   Shareability
                uint64_t af : 1;      // [10]    Access Flag
                uint64_t ng : 1;      // [11]    Not Global
                uint64_t paddr : 28;  // [39:12] Target physical address shifted by 12 (PFN)
                uint64_t res0_1 : 12; // [51:40] Reserved (0)
                uint64_t contig : 1;  // [52]    Contiguous hint
                uint64_t pxn : 1;     // [53]    Privileged Execute Never
                uint64_t xn : 1;      // [54]    Execute Never
                uint64_t res0_2 : 9;  // [63:55] Reserved (0)
            } bits;

            constexpr descriptor() noexcept : raw(0) {}
            constexpr explicit descriptor(uint64_t v) noexcept : raw(v) {}

            // --------------------------------------------------------------------
            // Type-Safe Descriptor Constructors
            // --------------------------------------------------------------------

            // Alias for the 4KB Host Page Frame Number[cite: 7]
            using host_pfn_4k = structo::phys_pfn<structo::host_phys_space, structo::page_4k, uint64_t>;

            /**
             * @brief Constructs a Next-Level Table pointer.
             * Enforces that the target address belongs to the Host Physical Space[cite: 8].
             */
            template <typename T>
            static constexpr descriptor make_table(structo::phys_addr<T, structo::host_phys_space, uint64_t> next_level_paddr) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_TABLE;

                // from_addr automatically shifts the address down by page_shift (12)[cite: 7, 9]
                d.bits.paddr = host_pfn_4k::from_addr(next_level_paddr).value;
                return d;
            }

            /**
             * @brief Constructs a 2MB Block mapping (Level 2).
             * Enforces that the target address belongs to the Host Physical Space[cite: 8].
             */
            template <typename T>
            static constexpr descriptor make_block(structo::phys_addr<T, structo::host_phys_space, uint64_t> paddr, uint64_t attridx, uint64_t ap, bool xn = false) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_BLOCK;
                d.bits.attridx = attridx;
                d.bits.ns = 0;
                d.bits.ap = ap;
                d.bits.sh = SH_INNER_SHAREABLE;
                d.bits.af = 1;
                d.bits.paddr = host_pfn_4k::from_addr(paddr).value; // [39:12] covers block bases perfectly[cite: 7]
                d.bits.xn = xn ? 1 : 0;
                return d;
            }

            /**
             * @brief Constructs a terminal 4KB Page mapping (Level 3).
             * Enforces that the target address belongs to the Host Physical Space[cite: 8].
             */
            template <typename T>
            static constexpr descriptor make_page(structo::phys_addr<T, structo::host_phys_space, uint64_t> paddr, uint64_t attridx, uint64_t ap, bool xn = false) noexcept
            {
                descriptor d;
                d.bits.type = TYPE_PAGE;
                d.bits.attridx = attridx;
                d.bits.ns = 0;
                d.bits.ap = ap;
                d.bits.sh = SH_INNER_SHAREABLE;
                d.bits.af = 1;
                d.bits.paddr = host_pfn_4k::from_addr(paddr).value; // Extracts bits [39:12] securely[cite: 7]
                d.bits.xn = xn ? 1 : 0;
                return d;
            }
        };

        static_assert(sizeof(descriptor) == 8, "LPAE descriptor must be exactly 8 bytes");

    } // namespace lpae_stage1
} // namespace microvisor