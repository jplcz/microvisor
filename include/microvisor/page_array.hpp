// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <cstddef>
#include <reloco/phys_addr.hpp>
#include <reloco/error.hpp>
#include <reloco/region_set.hpp>
#include <microvisor/memory_map.hpp>
#include <reloco/intrusive_c_tailq.hpp>

namespace microvisor
{

    struct page_descriptor
    {
        uint16_t flags;
        uint8_t order;
        uint8_t refcount;
        reloco::detail::c_tailq_hook_layout<page_descriptor> link;
    };

    struct page_freelist : reloco::c_tailq<page_descriptor, &page_descriptor::link>
    {
        void push_front(page_descriptor *ps) noexcept
        {
            RELOCO_ASSERT(ps, "null pointer");
            reloco::c_tailq<page_descriptor, &page_descriptor::link>::push_front(*ps);
        }
        void remove(page_descriptor *ps) noexcept
        {
            RELOCO_ASSERT(ps, "null pointer");
            reloco::c_tailq<page_descriptor, &page_descriptor::link>::remove(*ps);
        }
    };

    constexpr uint16_t PAGE_FLAG_FREE = 0;
    constexpr uint16_t PAGE_FLAG_RESERVED = (1 << 0);
    constexpr uint16_t PAGE_FLAG_ALLOCATED = (1 << 1);

    class page_array
    {
    public:
        static constexpr uint32_t PAGE_SIZE = 4096;
        static constexpr uint32_t PAGE_SHIFT = 12;

        constexpr page_array() noexcept = default;

        /**
         * @brief Constructs a unified page array using the dual memory map.
         */
        static reloco::result<page_array> create(const microvisor::memory_map &mem) noexcept
        {
            if (mem.total.empty() || mem.free.empty())
            {
                return reloco::unexpected(reloco::error::invalid_argument);
            }

            // Find absolute bounds directly from the Total RAM set
            uint32_t min_paddr = mem.total[0].base;
            uint32_t max_paddr = mem.total[mem.total.size() - 1].end();

            // Page-align the bounds
            min_paddr = min_paddr & ~(PAGE_SIZE - 1);
            max_paddr = (max_paddr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            size_t total_pages = (max_paddr - min_paddr) >> PAGE_SHIFT;
            size_t array_bytes = total_pages * sizeof(page_descriptor);

            // Find a memory region large enough in the FREE set to host the array
            auto largest = mem.free.largest_region();
            if (largest.size < array_bytes)
            {
                return reloco::unexpected(reloco::error::allocation_failed);
            }

            uint32_t array_paddr = (largest.base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            auto *descriptors = reinterpret_cast<page_descriptor *>(array_paddr);

            // Default EVERYTHING to RESERVED.
            for (size_t i = 0; i < total_pages; ++i)
            {
                descriptors[i] = page_descriptor{PAGE_FLAG_RESERVED, 0, 0, {}};
            }

            // Mark only the valid, usable regions from the FREE set as FREE
            for (size_t i = 0; i < mem.free.size(); ++i)
            {
                uint32_t r_base = (mem.free[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                uint32_t r_end = mem.free[i].end() & ~(PAGE_SIZE - 1);

                if (r_base >= r_end)
                    continue;

                size_t start_idx = (r_base - min_paddr) >> PAGE_SHIFT;
                size_t end_idx = (r_end - min_paddr) >> PAGE_SHIFT;

                for (size_t j = start_idx; j < end_idx; ++j)
                {
                    descriptors[j].flags = PAGE_FLAG_FREE;
                }
            }

            // Re-reserve the memory used by the descriptor array itself
            size_t array_start_idx = (array_paddr - min_paddr) >> PAGE_SHIFT;
            size_t array_pages = (array_bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;

            for (size_t i = 0; i < array_pages; ++i)
            {
                descriptors[array_start_idx + i].flags = PAGE_FLAG_RESERVED;
            }

            return page_array(descriptors, min_paddr, total_pages);
        }

        constexpr bool is_valid() const noexcept { return m_array != nullptr; }
        constexpr size_t size() const noexcept { return m_count; }

        constexpr page_descriptor *begin() const noexcept { return m_array; }
        constexpr page_descriptor *end() const noexcept { return m_array + m_count; }

        constexpr page_descriptor *paddr_to_page(uint32_t paddr) const noexcept
        {
            if (paddr < m_base_paddr || paddr >= (m_base_paddr + (m_count << PAGE_SHIFT)))
            {
                return nullptr;
            }
            return &m_array[(paddr - m_base_paddr) >> PAGE_SHIFT];
        }

        constexpr uint32_t page_to_paddr(const page_descriptor *page) const noexcept
        {
            return m_base_paddr + ((page - m_array) << PAGE_SHIFT);
        }

    private:
        constexpr page_array(page_descriptor *arr, uint32_t base, size_t count)
            : m_array(arr), m_base_paddr(base), m_count(count) {}

        page_descriptor *m_array{nullptr};
        uint32_t m_base_paddr{0};
        size_t m_count{0};
    };

} // namespace microvisor