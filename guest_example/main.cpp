// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

// A tiny example Guest OS payload, built as its own freestanding C++23
// executable (reusing reloco/microfmt exactly like the hypervisor does)
// instead of being hand-assembled into a raw array of ARM opcodes in
// src/main.cpp. See guest_example/CMakeLists.txt for how the resulting
// flat binary gets embedded back into microvisor.elf.

#include <microfmt/microfmt.hpp>
#include <reloco/default_allocator.hpp>
#include <reloco/stack_allocator.hpp>
#include <reloco/vector.hpp>
#include <reloco/string.hpp>
#include <algorithm>
#include <cstdint>

namespace
{
    // MMIO base addresses of the virtual devices the hypervisor registers
    // for this VM -- see the `register_device` calls in src/main.cpp.
    constexpr uintptr_t POWER_BASE = 0x80000000;
    constexpr uintptr_t CONSOLE_BASE = 0x80001000;

    constexpr uintptr_t POWER_REG_STATUS = POWER_BASE + 0x00;
    constexpr uintptr_t POWER_REG_HALT = POWER_BASE + 0x04;
    constexpr uintptr_t CONSOLE_REG_DATA = CONSOLE_BASE + 0x00;

    // Matches microvisor::devices::power_device::write's expected payload.
    constexpr uint32_t POWER_HALT_MAGIC = 0xCAFEBABE;

    /**
     * @brief Minimal `microfmt::sink` adapter for the hypervisor's emulated
     * `simple_console` MMIO device: one byte-store to `REG_DATA` per
     * character, modeled after `microfmt::pl011_sink`.
     */
    class console_sink
    {
    public:
        [[nodiscard]] constexpr microfmt::sink as_sink() const noexcept
        {
            return microfmt::sink{const_cast<void *>(static_cast<const void *>(this)), &console_sink::write_impl};
        }

    private:
        static void write_impl(void *, microfmt::string_view str) noexcept
        {
            volatile auto *data_reg = reinterpret_cast<volatile uint8_t *>(CONSOLE_REG_DATA);
            for (char c : str)
            {
                *data_reg = static_cast<uint8_t>(c);
            }
        }
    };

    // A tiny bump-allocated heap for the guest's own container usage, backed
    // by reloco's `stack_allocator` (see reloco/stack_allocator.hpp): no
    // free(), just a linear arena that individual tests carve `vector`s and
    // `string`s out of. 8 KiB is plenty for the tests below and comfortably
    // fits inside the 2 MiB Guest RAM block the hypervisor allocates for us.
    alignas(alignof(std::max_align_t)) std::byte g_heap_buffer[8 * 1024];

    // Set by main() once the arena above is constructed. `default_allocator`
    // (below) is only there to satisfy reloco's global-allocator hook link
    // requirement -- every container in this file is explicitly allocated
    // against `heap.ref()`, so this is never actually exercised, and it's
    // fine for it to hand back an empty/invalid `allocator_ref` before
    // `main()` runs.
    reloco::allocator_ref *g_default_allocator = nullptr;

    /**
     * @brief Builds a vector of the first `count` squares (0, 1, 4, 9, ...),
     * sums them, and checks the result against the closed-form sum of
     * squares formula -- a simple end-to-end exercise of allocation, growth
     * (`try_push_back`) and indexed access (`operator[]`) on the heap.
     */
    bool test_vector_sum(reloco::allocator_ref heap, microfmt::sink log)
    {
        constexpr uint32_t count = 64;

        auto vec_res = reloco::vector<uint32_t>::try_allocate(heap, count);
        if (!vec_res)
        {
            microfmt::format_to(log, "  [FAIL] vector allocation failed\r\n");
            return false;
        }
        auto &vec = vec_res.unwrap();

        for (uint32_t i = 0; i < count; ++i)
        {
            vec.try_push_back(i * i).unwrap();
        }

        uint64_t sum = 0;
        for (uint32_t i = 0; i < vec.size(); ++i)
        {
            sum += vec[i];
        }

        // Sum of squares 0..n-1 == (n-1)*n*(2n-1)/6
        const uint64_t expected = (static_cast<uint64_t>(count - 1) * count * (2 * count - 1)) / 6;
        const bool ok = sum == expected && vec.size() == count;
        microfmt::format_to(log, "  [{}] vector<uint32_t> sum-of-squares: {} (expected {})\r\n",
                             ok ? "PASS" : "FAIL", sum, expected);
        return ok;
    }

    /**
     * @brief Fills a vector with a reversed sequence, sorts it in place with
     * `std::sort` (exercising the container's random-access iterators), and
     * checks it comes out ascending.
     */
    bool test_vector_sort(reloco::allocator_ref heap, microfmt::sink log)
    {
        constexpr uint32_t count = 32;

        auto vec_res = reloco::vector<uint32_t>::try_allocate(heap, count);
        if (!vec_res)
        {
            microfmt::format_to(log, "  [FAIL] vector allocation failed\r\n");
            return false;
        }
        auto &vec = vec_res.unwrap();

        for (uint32_t i = 0; i < count; ++i)
        {
            vec.try_push_back(count - i).unwrap();
        }

        std::sort(vec.begin(), vec.end());

        bool ok = true;
        for (uint32_t i = 0; i < count; ++i)
        {
            ok = ok && vec[i] == i + 1;
        }
        microfmt::format_to(log, "  [{}] vector<uint32_t> std::sort: first={} last={}\r\n", ok ? "PASS" : "FAIL",
                             vec.front(), vec.back());
        return ok;
    }

    /**
     * @brief Builds a `reloco::string` on the guest heap via `format_to`-style
     * incremental writes and checks the resulting contents/length.
     */
    bool test_string_build(reloco::allocator_ref heap, microfmt::sink log)
    {
        auto str_res = reloco::string::try_allocate(heap);
        if (!str_res)
        {
            microfmt::format_to(log, "  [FAIL] string allocation failed\r\n");
            return false;
        }
        auto &str = str_res.unwrap();

        for (const char *word : {"micro", "visor", "-", "guest"})
        {
            for (const char *p = word; *p != '\0'; ++p)
            {
                str.try_push_back(*p).unwrap();
            }
        }

        const bool ok = str.view() == reloco::string_view("microvisor-guest");
        microfmt::format_to(log, "  [{}] reloco::string build: \"{}\" (len {})\r\n", ok ? "PASS" : "FAIL",
                             str.view(), str.size());
        return ok;
    }
    /**
     * @brief Reads the AArch32 Virtual Count register (CNTVCT), a
     * monotonically increasing 64-bit tick counter driven by the ARM
     * Generic Timer -- readable directly from PL1 (no Hyp-mode setup
     * required, unlike the physical counter/timer which the hypervisor
     * gates via CNTHCTL; see include/microvisor/timer.hpp).
     */
    inline uint64_t read_cntvct() noexcept
    {
        uint32_t lo, hi;
        asm volatile("mrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }

    /**
     * @brief Reads the Counter Frequency register (CNTFRQ) in Hz.
     */
    inline uint32_t read_cntfrq() noexcept
    {
        uint32_t freq;
        asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(freq));
        return freq;
    }

    /**
     * @brief Busy-waits on CNTVCT until at least `target_ms` milliseconds
     * have elapsed (per CNTFRQ), checking along the way that the counter is
     * monotonically non-decreasing and that it doesn't need an unreasonable
     * number of iterations to reach the target -- catching a stuck/broken
     * timer instead of hanging forever.
     */
    bool test_timer_loop(microfmt::sink log)
    {
        constexpr uint32_t target_ms = 100;
        constexpr uint64_t max_iterations = 200'000'000ULL;

        const uint32_t freq = read_cntfrq();
        if (freq == 0)
        {
            microfmt::format_to(log, "  [FAIL] CNTFRQ reads 0\r\n");
            return false;
        }

        const uint64_t target_ticks = (static_cast<uint64_t>(freq) * target_ms) / 1000ULL;
        const uint64_t start = read_cntvct();

        uint64_t last = start;
        uint64_t iterations = 0;
        bool monotonic = true;
        while (read_cntvct() - start < target_ticks)
        {
            const uint64_t now = read_cntvct();
            monotonic = monotonic && now >= last;
            last = now;

            if (++iterations > max_iterations)
            {
                microfmt::format_to(log, "  [FAIL] timer loop: CNTVCT did not advance after {} iterations\r\n",
                                     iterations);
                return false;
            }
        }

        const uint64_t end = read_cntvct();
        const uint64_t elapsed_ticks = end - start;
        const uint64_t elapsed_ms = (elapsed_ticks * 1000ULL) / freq;

        const bool ok = monotonic && elapsed_ms >= target_ms;
        microfmt::format_to(log, "  [{}] CNTVCT timer loop: waited {}ms (freq {}Hz, {} ticks, monotonic={})\r\n",
                             ok ? "PASS" : "FAIL", elapsed_ms, freq, elapsed_ticks, monotonic);
        return ok;
    }

    /**
     * @brief Longer-running soak test: busy-waits on CNTVCT for several
     * seconds, printing one heartbeat line per elapsed second, then checks
     * the whole run landed close to the expected wall-clock duration.
     * Exercises the timer continuously rather than just once, and gives a
     * visible sign of life for longer QEMU runs.
     */
    bool test_heartbeat_loop(microfmt::sink log)
    {
        constexpr uint32_t seconds = 5;
        constexpr uint64_t max_iterations_per_tick = 500'000'000ULL;

        const uint32_t freq = read_cntfrq();
        if (freq == 0)
        {
            microfmt::format_to(log, "  [FAIL] CNTFRQ reads 0\r\n");
            return false;
        }

        const uint64_t start = read_cntvct();
        bool ok = true;

        for (uint32_t second = 1; second <= seconds; ++second)
        {
            const uint64_t target_ticks = static_cast<uint64_t>(freq) * second;

            uint64_t iterations = 0;
            while (read_cntvct() - start < target_ticks)
            {
                if (++iterations > max_iterations_per_tick)
                {
                    microfmt::format_to(log, "  [FAIL] heartbeat {}/{}: CNTVCT stalled\r\n", second, seconds);
                    return false;
                }
            }

            const uint64_t elapsed_ms = ((read_cntvct() - start) * 1000ULL) / freq;
            microfmt::format_to(log, "  [VM 1] heartbeat {}/{} @ {}ms\r\n", second, seconds, elapsed_ms);
        }

        const uint64_t total_ms = ((read_cntvct() - start) * 1000ULL) / freq;
        // Allow generous slack above the target since this also accounts for
        // however long the hypervisor takes to service/reschedule the VM
        // between our CNTVCT polls, not just raw busy-wait overhead.
        ok = total_ms >= static_cast<uint64_t>(seconds) * 1000 && total_ms < static_cast<uint64_t>(seconds) * 5000;
        microfmt::format_to(log, "  [{}] heartbeat loop: total {}ms for {} beats\r\n", ok ? "PASS" : "FAIL", total_ms,
                             seconds);
        return ok;
    }
} // namespace

reloco::allocator_ref reloco::reloco_global_alloc::default_allocator() noexcept
{
    if (g_default_allocator == nullptr)
        return {};
    return *g_default_allocator;
}

// reloco's RELOCO_KERNEL_PANIC hook (see reloco_porting/reloco_user_config.hpp)
// -- required because this guest links against the same reloco headers as
// the hypervisor. Reports the panic over the emulated console, then halts
// the VM instead of returning (there is nowhere sensible to unwind to).
void do_panic(const char *expr, const char *file, int line, const char *message)
{
    console_sink console;
    microfmt::format_to(console.as_sink(), "Guest panic @ {}:{} in {}: {}\r\n", file, line, expr, message);

    volatile auto *power_halt = reinterpret_cast<volatile uint32_t *>(POWER_REG_HALT);
    *power_halt = POWER_HALT_MAGIC;

    for (;;)
    {
    }
}

int main()
{
    // Read the Power device's "VM01" signature, matching the previous
    // hand-encoded demo payload's behavior.
    volatile auto *power_status = reinterpret_cast<volatile uint32_t *>(POWER_REG_STATUS);
    [[maybe_unused]] uint32_t signature = *power_status;

    console_sink console;
    const auto log = console.as_sink();
    microfmt::format_to(log, "[VM 1] MMIO Bus Online!\r\n");

    // Stand up a simple bump-allocated heap and run a handful of real
    // container/algorithm tests over it, instead of just printing a static
    // greeting -- exercises reloco::vector/string + std::sort on top of
    // reloco::stack_allocator, all inside the guest.
    reloco::stack_allocator heap(reloco::stack_allocator_context(g_heap_buffer, sizeof(g_heap_buffer)));
    auto heap_ref = heap.ref();
    g_default_allocator = &heap_ref;

    microfmt::format_to(log, "[VM 1] Running heap tests...\r\n");
    bool all_ok = true;
    all_ok = test_vector_sum(heap.ref(), log) && all_ok;
    all_ok = test_vector_sort(heap.ref(), log) && all_ok;
    all_ok = test_string_build(heap.ref(), log) && all_ok;
    all_ok = test_timer_loop(log) && all_ok;
    all_ok = test_heartbeat_loop(log) && all_ok;
    microfmt::format_to(log, "[VM 1] Heap tests: {}\r\n", all_ok ? "ALL PASSED" : "FAILURE");

    // Ask the hypervisor to halt the VM.
    volatile auto *power_halt = reinterpret_cast<volatile uint32_t *>(POWER_REG_HALT);
    *power_halt = POWER_HALT_MAGIC;

    // The hypervisor should have stopped scheduling us by now; spin just in
    // case the halt request is ever ignored.
    for (;;)
    {
    }
}
