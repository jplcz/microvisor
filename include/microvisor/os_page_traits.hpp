// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "microvisor/page_array.hpp"
#include <structo/phys_page.hpp>
#include <reloco/error.hpp>
#include <structo/buddy_allocator.hpp>

namespace microvisor
{

    // Provide global access to the active page array for PFN resolution
    extern page_array g_pages;

    /**
     * @brief Bridges microvisor's page_descriptor to reloco's os_traits_base.
     */
    struct os_page_traits : structo::os_traits_base<os_page_traits, page_descriptor *>
    {
        using os_page_type = page_descriptor *;

        [[nodiscard]] static os_page_type null_page() noexcept { return nullptr; }

        [[nodiscard]] static bool is_null(os_page_type p) noexcept { return p == nullptr; }

        [[nodiscard]] static uint64_t to_pfn(os_page_type p) noexcept
        {
            // Calculate the physical address and shift down by 12 (4K page shift)
            return g_pages.page_to_paddr(p) >> page_array::PAGE_SHIFT;
        }

        [[nodiscard]] static reloco::result<os_page_type> from_pfn(uint64_t pfn) noexcept
        {
            // Shift up to form the physical address
            uint32_t paddr = static_cast<uint32_t>(pfn << page_array::PAGE_SHIFT);
            page_descriptor *pd = g_pages.paddr_to_page(paddr);

            if (!pd)
            {
                return reloco::unexpected(reloco::error::out_of_range);
            }
            return pd;
        }

        [[nodiscard]] static bool is_same_zone(os_page_type, os_page_type) noexcept
        {
            // In a flat physical memory model on the QEMU virt board, all RAM is one zone.
            return true;
        }

        [[nodiscard]] static uint16_t buddy_order(os_page_type p) noexcept
        {
            return p->order;
        }

        static void set_buddy_order(os_page_type p, uint16_t order) noexcept
        {
            p->order = static_cast<uint8_t>(order);
        }

        [[nodiscard]] static bool is_buddy_free(os_page_type p) noexcept
        {
            return p->flags == PAGE_FLAG_FREE;
        }

        static void set_buddy_free(os_page_type p, bool is_free) noexcept
        {
            if (is_free)
            {
                p->flags = PAGE_FLAG_FREE;
            }
            else
            {
                p->flags = PAGE_FLAG_ALLOCATED;
            }
        }
    };

    /**
     * @brief A strictly typed page view combining reloco's 4K traits with our OS traits.
     */
    using page_view_4k = structo::page_view<structo::page_4k, os_page_traits>;

    extern structo::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k> g_buddy;

} // namespace microvisor