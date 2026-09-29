#include <microfmt/hw/pl011_sink.hpp>
#include <microfmt/microfmt.hpp>
#include <reloco/region_set.hpp>
#include <reloco/buddy_allocator.hpp>
#include <reloco/default_allocator.hpp>
#include <reloco/bucket_allocator.hpp>
#include <reloco/spin_lock.hpp>
#include <reloco/once_lock.hpp>
#include <reloco/unique_ptr.hpp>
#include <reloco/instant.hpp>
#include <microfmt/log/logger.hpp>
#include <microfmt/formatters/pointer.hpp>

#include <microvisor/page_array.hpp>
#include <microvisor/hyp.hpp>
#include <microvisor/memory_map.hpp>
#include <microvisor/os_page_traits.hpp>
#include <microvisor/buddy_upstream.hpp>
#include <microvisor/lpae_manager.hpp>
#include <microvisor/mmu.hpp>
#include <microvisor/vm.hpp>
#include <microfmt/log/macros.hpp>
#include <microvisor/trap_handler.hpp>
#include <microvisor/cache.hpp>

#include <microvisor/devices/power_device.hpp>
#include <microvisor/devices/simple_console.hpp>

#include <reloco/fdt_reader.hpp>
#include <reloco/fdt_memory.hpp>

#include <microvisor/boot.hpp>

#include <cstring>

extern "C"
{
    extern uint8_t _start[]; // Now resolved to 0x40010000
    extern uint8_t __bss_end[];
    extern uint8_t __stack_top[];

    // The guest_example payload (see guest_example/), flattened to a raw
    // binary and embedded into this executable by objcopy at link time.
    extern const uint8_t _binary_guest_example_bin_start[];
    extern const uint8_t _binary_guest_example_bin_end[];
}

microvisor::page_array microvisor::g_pages;
reloco::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k> microvisor::g_buddy;

void do_panic(const char *expr, const char *file, int line, const char *mesage)
{
    microfmt::pl011_sink uart(0x09000000, /*translate_crlf=*/true);
    const auto sink = uart.as_sink();
    microfmt::format_to(sink, "Panic @ {}:{} in {}: {}\n", file, line, expr, mesage);
}

/**
 * @brief Constructs the initial safe physical memory map for the hypervisor.
 */
reloco::result<microvisor::memory_map> bootstrap_memory_regions() noexcept
{
    microvisor::memory_map map;

    if (g_dtb_ptr == nullptr)
        return reloco::unexpected(reloco::error::invalid_argument);

    MICROFMT_LOG_INFO("DTB @ {:#x}", reinterpret_cast<uintptr_t>(g_dtb_ptr));

    const auto *dtb_bytes = reinterpret_cast<const std::byte *>(g_dtb_ptr);

    // Peek at just the header to learn the blob's actual declared
    // `totalsize`, rather than assuming a fixed upper bound.
    auto size_res = reloco::fdt::fdt_reader::try_probe_size(
        reloco::span<const std::byte>(dtb_bytes, 64));
    if (!size_res)
        return reloco::unexpected(size_res.error());

    MICROFMT_LOG_INFO("DTB size {:#x}", *size_res);

    auto reader_res = reloco::fdt::fdt_reader::try_create(
        reloco::span<const std::byte>(dtb_bytes, *size_res));
    if (!reader_res)
        return reloco::unexpected(reader_res.error());

    // Populate `total`/`free` straight from the devicetree's `/memory` and
    // `/reserved-memory` nodes instead of a hardcoded 128 MB assumption.
    auto res = reloco::fdt::try_extract_memory(*reader_res, map.total, map.free);
    if (!res)
        return reloco::unexpected(res.error());

    map.total.iter().for_each([&](const auto &region)
                              { MICROFMT_LOG_INFO("FDT TOTAL {:#x} -- {:#x}", region.base, region.base + region.size); });

    map.free.iter().for_each([&](const auto &region)
                             { MICROFMT_LOG_INFO("FDT FREE {:#x} -- {:#x}", region.base, region.base + region.size); });

    // Punch out the Boot ROM / FDT region
    MICROFMT_LOG_INFO("Punching Boot ROM / FDT region {:#x} -- {:#x}", 0x40000000, 0x40000000 + 0x10000);
    res = map.free.try_subtract(0x40000000, 0x10000);
    if (!res)
        return reloco::unexpected(res.error());

    // Punch out the Hypervisor Binary Footprint
    uintptr_t hv_start = reinterpret_cast<uintptr_t>(_start);
    uintptr_t hv_size = reinterpret_cast<uintptr_t>(__stack_top) - hv_start;
    MICROFMT_LOG_INFO("Punching Hypervisor footprint {:#x} -- {:#x}", hv_start, hv_start + hv_size);
    res = map.free.try_subtract(hv_start, hv_size);
    if (!res)
        return reloco::unexpected(res.error());

    return map;
}

using microvm_alloc = reloco::bucket_allocator<reloco::null_mutex, 32, 64, 128, 256, 512, 1024, 2048>;

static reloco::optional<microvm_alloc> s_allocator;
static reloco::optional<microvisor::budy_allocator_wrap> s_buddy_alloc;
reloco::optional<microvisor::lpae_manager> microvisor::g_stage1_mmu;

reloco::allocator_ref
reloco::reloco_global_alloc::default_allocator() noexcept
{
    if (!s_allocator.has_value())
        return {};
    return s_allocator->ref();
}

int main()
{
    MICROFMT_LOG_INFO("Checking HYP mode\n");

    if (!microvisor::is_hyp_mode())
    {
        // We booted in SVC mode (or lower).
        // We would need to execute an 'hvc' instruction to elevate here.
        MICROFMT_LOG_INFO("Not in HYP");
        return 0;
    }

    MICROFMT_LOG_INFO("Hello World!");

    microvisor::init_hvbar();

    MICROFMT_LOG_INFO("CPSR={:#x}", hyp_get_cpsr());

    MICROFMT_LOG_INFO("Bootstrapping memory regions...");
    const auto regions = bootstrap_memory_regions().unwrap();

    regions.free.iter().for_each([&](const auto &region)
                                 { MICROFMT_LOG_INFO("FREE {:#x} -- {:#x}", region.base, region.base + region.size); });

    regions.total.iter().for_each([&](const auto &region)
                                  { MICROFMT_LOG_INFO("TOTAL {:#x} -- {:#x}", region.base, region.base + region.size); });

    microvisor::g_pages = microvisor::page_array::create(regions).unwrap();

    MICROFMT_LOG_INFO("{} pages", microvisor::g_pages.size());

    MICROFMT_LOG_INFO("Initializing buddy allocator...");
    microvisor::g_buddy.init(microvisor::page_view_4k::from_os_page(
                                 microvisor::g_pages.begin()),
                             microvisor::g_pages.size())
        .unwrap();
    MICROFMT_LOG_INFO("Buddy allocator init done");

    s_buddy_alloc.emplace(microvisor::g_buddy);

    s_allocator.emplace(2 * sizeof(void *), s_buddy_alloc->ref(), 65536);

    microvisor::g_stage1_mmu.emplace(s_buddy_alloc->ref());

    MICROFMT_LOG_INFO("Initializing Stage-1 MMU...");
    microvisor::g_stage1_mmu->init().unwrap();

    MICROFMT_LOG_INFO("Init MMU");

    MICROFMT_LOG_INFO("Enabling Hypervisor MMU...");
    microvisor::enable_hypervisor_mmu(regions);

    MICROFMT_LOG_INFO("MMU init Done");

    // Static device instances
    microvisor::devices::power_device g_power_dev;
    microvisor::devices::simple_console g_console_dev(0x09000000); // Host PL011 base

    MICROFMT_LOG_INFO("Allocating VM...");
    auto my_vm = reloco::unique_ptr<microvisor::vm>::try_create(
                     1,                   // Passed to try_construct: VMID
                     s_buddy_alloc->ref() // Passed to try_construct: Page allocator
                     )
                     .unwrap();

    MICROFMT_LOG_INFO("VM {} successfully allocated!", my_vm->id());

    // Map Guest memory directly via operator->
    using guest_addr_t = reloco::phys_addr<void, reloco::guest_phys_space, uint64_t>;
    using host_addr_t = reloco::phys_addr<void, reloco::host_phys_space, uint64_t>;

    // Carve a 2 MB (512 * 4K pages => order 9) block of Host RAM out of the
    // buddy allocator instead of hardcoding a host physical address.
    MICROFMT_LOG_INFO("Allocating Guest RAM block...");
    constexpr size_t guest_ram_order = 9;
    auto guest_ram_block = microvisor::g_buddy.allocate(guest_ram_order).unwrap();
    uint32_t hpa_ram_paddr = microvisor::g_pages.page_to_paddr(guest_ram_block.get_os_page());
    MICROFMT_LOG_INFO("Guest RAM block @ {:#x}", hpa_ram_paddr);

    guest_addr_t gpa_ram{0x40000000};
    host_addr_t hpa_ram{hpa_ram_paddr};

    uint64_t vm_ram_desc = microvisor::lpae_stage2::descriptor::make_block(
                               hpa_ram,
                               microvisor::lpae_stage2::MEMATTR_NORMAL_WB,
                               microvisor::lpae_stage2::S2AP_RW,
                               false)
                               .raw;

    MICROFMT_LOG_INFO("Mapping Guest RAM into Stage-2...");
    my_vm->stage2().map_block_2m(gpa_ram, vm_ram_desc).unwrap();
    MICROFMT_LOG_INFO("Guest RAM mapped");

    // Map Power Device at [0x80000000 - 0x80000FFF]
    MICROFMT_LOG_INFO("Registering Power device...");
    my_vm->mmio().register_device(0x80000000, 0x1000, &g_power_dev);

    // Map Simple Console at [0x80001000 - 0x80001FFF]
    MICROFMT_LOG_INFO("Registering Console device...");
    my_vm->mmio().register_device(0x80001000, 0x1000, &g_console_dev);

    // Write the Guest Payload into the Host physical memory we assigned it.
    // We mapped guest 0x40000000 -> the freshly-allocated `hpa_ram_paddr`.
    // Because Stage-1 maps the whole 128MB 1:1, we can write directly there!
    // The payload itself is `guest_example.elf` (see guest_example/), built
    // as its own freestanding C++23 executable and embedded into this
    // binary at link time -- see the `_binary_guest_example_bin_*` symbols
    // declared above -- instead of a hand-encoded array of raw instructions.
    MICROFMT_LOG_INFO("Writing Guest payload...");
    void *guest_ram = reinterpret_cast<void *>(hpa_ram_paddr);
    const std::size_t guest_image_size =
        static_cast<std::size_t>(_binary_guest_example_bin_end - _binary_guest_example_bin_start);
    memcpy(guest_ram, _binary_guest_example_bin_start, guest_image_size);

    // Flush cache lines covering the copied image.
    MICROFMT_LOG_INFO("Flushing Guest payload cache lines...");
    auto *guest_ram_words = reinterpret_cast<uint32_t *>(guest_ram);
    microvisor::clear_cache(guest_ram_words, guest_ram_words + (guest_image_size + 3) / 4);

    // Configure VCPU to boot at the base of RAM in Supervisor mode
    my_vm->vcpu().pc = gpa_ram.value;
    my_vm->vcpu().cpsr = 0x000001D3; // SVC mode (0x13) + IRQ/FIQ disabled

    MICROFMT_LOG_INFO("Entering VM {}...", my_vm->id());

    bool vm_running = true;
    uint32_t vm_iteration = 0;
    while (vm_running)
    {
        // Enter Guest
        my_vm->run();

        // Dispatch the Exit
        microvisor::trap::exit_status status = microvisor::trap::dispatch_exit(*my_vm);
        ++vm_iteration;

        // Handle Dispatcher Result
        if (status == microvisor::trap::exit_status::halt)
        {
            vm_running = false;
        }
    }

    // We are back!
    MICROFMT_LOG_INFO("VM {} terminated cleanly. Final PC: {:#010x}",
                      my_vm->id(),
                      my_vm->vcpu().pc);

    return 0;
}

void operator delete(void *ptr) noexcept
{
    (void)ptr;
    RELOCO_ASSERT(false, "Impossible");
}

void operator delete[](void *ptr) noexcept
{
    (void)ptr;
    RELOCO_ASSERT(false, "Impossible");
}

void operator delete(void *ptr, std::size_t size) noexcept
{
    reloco::default_allocator().deallocate(ptr, size);
}

void operator delete[](void *ptr, std::size_t size) noexcept
{
    reloco::default_allocator().deallocate(ptr, size);
}

void operator delete(void *ptr, std::align_val_t al) noexcept
{
    (void)ptr;
    (void)al;
    RELOCO_ASSERT(false, "Impossible");
}

void operator delete(void *ptr, std::size_t size, std::align_val_t al) noexcept
{
    (void)ptr;
    (void)size;
    (void)al;
    reloco::default_allocator().deallocate(ptr, size);
}