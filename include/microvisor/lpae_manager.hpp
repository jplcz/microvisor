// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <reloco/allocator.hpp>
#include <reloco/error.hpp>
#include <reloco/expected.hpp>
#include <reloco/detail/compat.hpp>
#include <structo/phys_addr.hpp>
#include <structo/phys_page.hpp>
#include <structo/pfn_translator.hpp>

namespace microvisor
{

    /**
     * @brief Generic LPAE Page Table Manager utilizing reloco's type-safe physical memory primitives.
     */
    class lpae_manager
    {
    public:
        // Strongly typed pointer for the Host tables allocated by our buddy allocator
        using table_addr_t = structo::phys_addr<uint64_t, structo::host_phys_space, uint64_t>;

        // Page Frame Number (PFN) representation for seamless descriptor packing
        using pfn_t = structo::phys_pfn<structo::host_phys_space, structo::page_4k, uint64_t>;

        constexpr explicit lpae_manager(reloco::allocator_ref alloc) noexcept
            : m_alloc(alloc) {}

        reloco::result<void> init() noexcept
        {
            auto res = allocate_zeroed_table();
            if (!res)
                return reloco::unexpected(res.error());

            m_root_table = res.value();
            return {};
        }

        [[nodiscard]] table_addr_t root_paddr() const noexcept
        {
            return m_root_table;
        }

        /**
         * @brief Injects a raw 64-bit Block Descriptor (2MB) at Level 2.
         * @tparam SpaceTag The address space being mapped (e.g., guest_phys_space or host_phys_space).
         */
        template <typename T, typename SpaceTag>
        reloco::result<void> map_block_2m(structo::phys_addr<T, SpaceTag, uint64_t> vaddr, uint64_t raw_descriptor) noexcept
        {
            // Type-safe 2MB alignment check[cite: 7, 9]
            if (structo::page_math::offset<structo::page_2m>(vaddr) != 0)
            {
                return reloco::unexpected(reloco::error::invalid_argument);
            }

            uint32_t l1_idx = (vaddr.value >> 30) & 0x3;
            uint32_t l2_idx = (vaddr.value >> 21) & 0x1FF;

            RELOCO_BEGIN_UNSAFE_BUFFER_USAGE
            // Since we are 1:1 mapped in early boot, the physical address is identical to the virtual pointer
            uint64_t *root_ptr = reinterpret_cast<uint64_t *>(m_root_table.value);

            auto l2_res = ensure_table(root_ptr[l1_idx]);
            if (!l2_res)
                return reloco::unexpected(l2_res.error());

            uint64_t *l2_ptr = reinterpret_cast<uint64_t *>(l2_res.value().value);
            l2_ptr[l2_idx] = raw_descriptor;
            RELOCO_END_UNSAFE_BUFFER_USAGE

            return {};
        }

        /**
         * @brief Injects a raw 64-bit Page Descriptor (4KB) at Level 3.
         */
        template <typename T, typename SpaceTag>
        reloco::result<void> map_page_4k(structo::phys_addr<T, SpaceTag, uint64_t> vaddr, uint64_t raw_descriptor) noexcept
        {
            // Type-safe 4KB alignment check[cite: 7, 9]
            if (structo::page_math::offset<structo::page_4k>(vaddr) != 0)
            {
                return reloco::unexpected(reloco::error::invalid_argument);
            }

            uint32_t l1_idx = (vaddr.value >> 30) & 0x3;
            uint32_t l2_idx = (vaddr.value >> 21) & 0x1FF;
            uint32_t l3_idx = (vaddr.value >> 12) & 0x1FF;

            RELOCO_BEGIN_UNSAFE_BUFFER_USAGE
            uint64_t *root_ptr = reinterpret_cast<uint64_t *>(m_root_table.value);

            auto l2_res = ensure_table(root_ptr[l1_idx]);
            if (!l2_res)
                return reloco::unexpected(l2_res.error());

            uint64_t *l2_ptr = reinterpret_cast<uint64_t *>(l2_res.value().value);
            auto l3_res = ensure_table(l2_ptr[l2_idx]);
            if (!l3_res)
                return reloco::unexpected(l3_res.error());

            uint64_t *l3_ptr = reinterpret_cast<uint64_t *>(l3_res.value().value);
            l3_ptr[l3_idx] = raw_descriptor;
            RELOCO_END_UNSAFE_BUFFER_USAGE

            return {};
        }

        /**
         * @brief Maps a contiguous range using 4KB pages safely.
         */
        template <typename T, typename SpaceTag, typename DescriptorBuilder>
        reloco::result<void> map_range_4k(structo::phys_addr<T, SpaceTag, uint64_t> vaddr, uint32_t size, DescriptorBuilder &&builder) noexcept
        {
            // Round down start address and round up end address securely
            auto start_v = structo::page_math::align_down<structo::page_4k>(vaddr);
            uint32_t end_val = (vaddr.value + size + structo::page_4k::alignment_mask) & ~static_cast<uint32_t>(structo::page_4k::alignment_mask);

            for (uint32_t current_val = start_v.value; current_val < end_val; current_val += structo::page_4k::page_size)
            {
                structo::phys_addr<T, SpaceTag, uint32_t> current_v{current_val};
                uint64_t desc = builder(current_v);

                auto res = map_page_4k(current_v, desc);
                if (!res)
                    return res;
            }
            return {};
        }

    private:
        reloco::allocator_ref m_alloc;
        table_addr_t m_root_table{nullptr}; // Defaults to ~0 (is_null) via phys_addr[cite: 8]

        reloco::result<table_addr_t> allocate_zeroed_table() noexcept
        {
            RELOCO_BEGIN_UNSAFE_BUFFER_USAGE
            auto block_res = m_alloc.allocate(structo::page_4k::page_size, structo::page_4k::page_size);
            if (!block_res)
                return reloco::unexpected(block_res.error());

            uint64_t *table = static_cast<uint64_t *>(block_res.value().ptr);
            for (int i = 0; i < 512; ++i)
            {
                table[i] = 0;
            }

            return table_addr_t{reinterpret_cast<uint32_t>(table)};
            RELOCO_END_UNSAFE_BUFFER_USAGE
        }

        reloco::result<table_addr_t> ensure_table(uint64_t &desc) noexcept
        {
            constexpr uint64_t TYPE_MASK = 0x3;
            constexpr uint64_t TYPE_FAULT = 0x0;
            constexpr uint64_t TYPE_TABLE = 0x3;

            // PADDR Mask for LPAE 40-bit physical addresses (Bits [39:12])
            constexpr uint64_t PADDR_MASK = 0x000000FFFFFFF000ULL;

            uint64_t type = desc & TYPE_MASK;

            if (type == TYPE_FAULT)
            {
                auto table_res = allocate_zeroed_table();
                if (!table_res)
                    return reloco::unexpected(table_res.error());

                table_addr_t new_table_addr = table_res.value();

                // In LPAE, bits [39:12] of a Table Descriptor hold the target Physical Address.
                // This happens to be mathematically identical to the Page Frame Number (PFN) shifted back by 12.
                // We use the PFN translator to securely pack the bits.
                pfn_t table_pfn = pfn_t::from_addr(new_table_addr);

                desc = TYPE_TABLE | (table_pfn.value << structo::page_4k::page_shift);
                return new_table_addr;
            }
            else if (type == TYPE_TABLE)
            {
                // Unpack the physical address securely by routing it through the PFN type[cite: 7].
                uint64_t pfn_val = (desc & PADDR_MASK) >> structo::page_4k::page_shift;
                pfn_t table_pfn{pfn_val};

                return table_pfn.to_addr<uint64_t>();
            }
            else
            {
                return reloco::unexpected(reloco::error::invalid_argument);
            }
        }
    };

} // namespace microvisor