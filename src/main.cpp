#include <microfmt/hw/pl011_sink.hpp>
#include <microfmt/microfmt.hpp>
#include <structo/region_set.hpp>
#include <structo/buddy_allocator.hpp>
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

#include <structo/fdt_reader.hpp>
#include <structo/fdt_memory.hpp>

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
structo::buddy_allocator<microvisor::page_freelist, microvisor::page_view_4k> microvisor::g_buddy;

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
    auto size_res = structo::fdt::fdt_reader::try_probe_size(
        reloco::span<const std::byte>(dtb_bytes, 64));
    if (!size_res)
        return reloco::unexpected(size_res.error());

    MICROFMT_LOG_INFO("DTB size {:#x}", *size_res);

    auto reader_res = structo::fdt::fdt_reader::try_create(
        reloco::span<const std::byte>(dtb_bytes, *size_res));
    if (!reader_res)
        return reloco::unexpected(reader_res.error());

    // Populate `total`/`free` straight from the devicetree's `/memory` and
    // `/reserved-memory` nodes instead of a hardcoded 128 MB assumption.
    auto res = structo::fdt::try_extract_memory(*reader_res, map.total, map.free);
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

    // Map Guest memory directly via operator->
    using guest_addr_t = structo::phys_addr<void, structo::guest_phys_space, uint64_t>;
    using host_addr_t = structo::phys_addr<void, structo::host_phys_space, uint64_t>;

    // Spin up two independent instances of the very same guest_example
    // template payload. Each VM gets its own VMID (so Stage-2 TLB entries
    // stay tagged apart -- see vm::run()'s VTTBR construction), its own
    // Stage-2 tables, its own carved-out RAM block, and its own vcpu_context
    // (whose guest system registers are lazily saved/restored per
    // microvisor/sysregs.hpp whenever the round-robin loop below switches
    // between them). The Power/Console devices are stateless enough to be
    // shared across both VMs' MMIO buses.
    constexpr uint8_t vm_count = 3;
    reloco::unique_ptr<microvisor::vm> vms[vm_count];

    auto setup_vm = [&](uint8_t vmid)
    {
        MICROFMT_LOG_INFO("Allocating VM {}...", vmid);
        auto &slot = vms[vmid - 1];
        slot = reloco::unique_ptr<microvisor::vm>::try_create(
                   vmid,                // Passed to try_construct: VMID
                   s_buddy_alloc->ref() // Passed to try_construct: Page allocator
                   )
                   .unwrap();

        MICROFMT_LOG_INFO("VM {} successfully allocated!", slot->id());

        // Carve a 2 MB (512 * 4K pages => order 9) block of Host RAM out of
        // the buddy allocator instead of hardcoding a host physical address.
        MICROFMT_LOG_INFO("Allocating Guest RAM block for VM {}...", vmid);
        constexpr size_t guest_ram_order = 9;
        auto guest_ram_block = microvisor::g_buddy.allocate(guest_ram_order).unwrap();
        uint32_t hpa_ram_paddr = microvisor::g_pages.page_to_paddr(guest_ram_block.get_os_page());
        MICROFMT_LOG_INFO("VM {} Guest RAM block @ {:#x}", vmid, hpa_ram_paddr);

        guest_addr_t gpa_ram{0x40000000};
        host_addr_t hpa_ram{hpa_ram_paddr};

        uint64_t vm_ram_desc = microvisor::lpae_stage2::descriptor::make_block(
                                   hpa_ram,
                                   microvisor::lpae_stage2::MEMATTR_NORMAL_WB,
                                   microvisor::lpae_stage2::S2AP_RW,
                                   false)
                                   .raw;

        MICROFMT_LOG_INFO("Mapping Guest RAM into Stage-2 for VM {}...", vmid);
        slot->stage2().map_block_2m(gpa_ram, vm_ram_desc).unwrap();

        // Map Power Device at [0x80000000 - 0x80000FFF]
        slot->mmio().register_device(0x80000000, 0x1000, &g_power_dev);

        // Map Simple Console at [0x80001000 - 0x80001FFF]
        slot->mmio().register_device(0x80001000, 0x1000, &g_console_dev);

        // Write the Guest Payload into the Host physical memory we assigned
        // it. We mapped guest 0x40000000 -> the freshly-allocated
        // `hpa_ram_paddr`. Because Stage-1 maps the whole 128MB 1:1, we can
        // write directly there! The payload itself is `guest_example.elf`
        // (see guest_example/), built as its own freestanding C++23
        // executable and embedded into this binary at link time -- see the
        // `_binary_guest_example_bin_*` symbols declared above -- instead of
        // a hand-encoded array of raw instructions. Both VMs get their own
        // fresh copy of the same blob.
        MICROFMT_LOG_INFO("Writing Guest payload for VM {}...", vmid);
        void *guest_ram = reinterpret_cast<void *>(hpa_ram_paddr);
        const std::size_t guest_image_size =
            static_cast<std::size_t>(_binary_guest_example_bin_end - _binary_guest_example_bin_start);
        memcpy(guest_ram, _binary_guest_example_bin_start, guest_image_size);

        // Flush cache lines covering the copied image.
        auto *guest_ram_words = reinterpret_cast<uint32_t *>(guest_ram);
        microvisor::clear_cache(guest_ram_words, guest_ram_words + (guest_image_size + 3) / 4);

        // Configure VCPU to boot at the base of RAM in Supervisor mode
        slot->vcpu().pc = gpa_ram.value;
        slot->vcpu().cpsr = 0x000001D3; // SVC mode (0x13) + IRQ/FIQ disabled
    };

    for (uint8_t i = 1; i <= vm_count; ++i)
        setup_vm(i);

    for (uint8_t i = 0; i < vm_count; ++i)
        MICROFMT_LOG_INFO("DEBUG VM {} pc={:#010x} cpsr={:#010x} vmid={}",
                          vms[i]->id(), vms[i]->vcpu().pc, vms[i]->vcpu().cpsr, vms[i]->id());

    MICROFMT_LOG_INFO("Entering {} VMs (round-robin, interleaved)...", vm_count);

    // Round-robin scheduler: give each still-running VM one time slice in
    // turn. Each vm::run() call arms the preemption timer for `slice_ms` and
    // returns either because the guest yielded/was preempted (`resume`, so
    // it stays in rotation) or halted (`halt`, so it's dropped). This is
    // exactly the scenario that exercises the sysregs.hpp lazy save/restore
    // switch-owner path: back-to-back slices of the *same* VM are free
    // (owner unchanged), but every round-robin hop to the *other* VM forces
    // a real flush + reload of the guest system registers.
    bool vm_running[vm_count];
    for (bool &running : vm_running)
        running = true;

    uint32_t running_count = vm_count;
    while (running_count > 0)
    {
        for (uint8_t i = 0; i < vm_count; ++i)
        {
            if (!vm_running[i])
                continue;

            microvisor::vm &current = *vms[i];

            // Enter Guest
            current.run();

            if (i == 1)
                MICROFMT_LOG_INFO("DEBUG VM {} exit pc={:#010x} cpsr={:#010x} exit_vector={:#x} hsr={:#010x} lr={:#010x} r0={:#010x}",
                                   current.id(), current.vcpu().pc, current.vcpu().cpsr, current.vcpu().exit_vector, hyp_get_hsr(),
                                   current.vcpu().lr, current.vcpu().r[0]);

            // Dispatch the Exit
            microvisor::trap::exit_status status = microvisor::trap::dispatch_exit(current);

            // Handle Dispatcher Result
            if (status == microvisor::trap::exit_status::halt)
            {
                MICROFMT_LOG_INFO("VM {} terminated cleanly. Final PC: {:#010x}",
                                  current.id(),
                                  current.vcpu().pc);
                vm_running[i] = false;
                --running_count;
            }
        }
    }

    MICROFMT_LOG_INFO("All {} VMs terminated.", vm_count);

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