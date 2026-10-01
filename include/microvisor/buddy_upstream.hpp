#pragma once

#include "os_page_traits.hpp"
#include "page_array.hpp"

namespace microvisor
{

    struct buddy_allocator_tag
    {
    };

} // namespace microvisor

namespace reloco
{

    /**
     * @brief Specialization providing the allocator backend for buddy_allocator_tag[cite: 6].
     */
    template <>
    struct allocator_traits<microvisor::buddy_allocator_tag>
    {
        using context_type = structo::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k>;

        /**
         * @brief Allocates a block of at least `bytes` size, aligned to `alignment`[cite: 6].
         */
        [[nodiscard]] static result<mem_block> allocate(value_ref<context_type> ctx,
                                                        std::size_t bytes, std::size_t alignment) noexcept
        {

            // Calculate how many 4K pages are needed
            std::size_t pages = (bytes + microvisor::page_array::PAGE_SIZE - 1) / microvisor::page_array::PAGE_SIZE;

            // Map the page count to a buddy order: 2^order >= pages
            uint8_t order = 0;
            while ((1ULL << order) < pages)
            {
                order++;
            }

            // Ensure buddy constraints satisfy alignment requirements.
            // A buddy block of order N is naturally aligned to (1 << (N + 12)) bytes.
            while ((1ULL << (order + microvisor::page_array::PAGE_SHIFT)) < alignment && order <= context_type::MAX_ORDER)
            {
                order++;
            }

            auto res = ctx->allocate(order);
            if (!res)
            {
                return unexpected(res.error());
            }

            microvisor::page_descriptor *pd = res->get_os_page();

            RELOCO_BEGIN_UNSAFE_BUFFER_USAGE; // Raw memory management block
            void *ptr = reinterpret_cast<void *>(microvisor::g_pages.page_to_paddr(pd));
            RELOCO_END_UNSAFE_BUFFER_USAGE;

            // Return the block with its actual available capacity, which is guaranteed
            // to be greater than or equal to the requested size
            std::size_t actual_capacity = static_cast<std::size_t>(1ULL << (order + microvisor::page_array::PAGE_SHIFT));

            return mem_block{ptr, actual_capacity};
        }

        /**
         * @brief Deallocates a previously allocated block
         */
        static void deallocate(value_ref<context_type> ctx, void *ptr, std::size_t bytes) noexcept
        {
            if (ptr == nullptr)
            {
                return;
            }

            std::size_t pages = (bytes + microvisor::page_array::PAGE_SIZE - 1) / microvisor::page_array::PAGE_SIZE;

            uint8_t order = 0;
            while ((1ULL << order) < pages)
            {
                order++;
            }

            RELOCO_BEGIN_UNSAFE_BUFFER_USAGE; // Raw memory management block
            uint32_t paddr = reinterpret_cast<uint32_t>(ptr);
            auto pd = microvisor::g_pages.paddr_to_page(paddr);
            RELOCO_ASSERT(pd != nullptr, "Bad page index");
            RELOCO_END_UNSAFE_BUFFER_USAGE;

            ctx->free(microvisor::page_view_4k::from_os_page(pd), order);
        }
    };

} // namespace reloco

namespace microvisor
{

    class budy_allocator_wrap
    {
    public:
        constexpr budy_allocator_wrap(structo::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k> &ctx) noexcept
            : context_(ctx) {}

        budy_allocator_wrap(const budy_allocator_wrap &) = delete;
        budy_allocator_wrap &operator=(const budy_allocator_wrap &) = delete;
        budy_allocator_wrap(budy_allocator_wrap &&) = delete;
        budy_allocator_wrap &operator=(budy_allocator_wrap &&) = delete;

        [[nodiscard]] constexpr reloco::allocator_ref ref() & noexcept RELOCO_LIFETIMEBOUND
        {
            return reloco::allocator_ref(buddy_allocator_tag{}, context_);
        }

        reloco::allocator_ref ref() && = delete;

    private:
        structo::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k> &context_;
    };

}