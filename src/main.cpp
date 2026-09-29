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

extern "C"
{
    extern uint8_t _start[]; // Now resolved to 0x40010000
    extern uint8_t __bss_end[];
    extern uint8_t __stack_top[];
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

    // Declare total physical RAM (128 MB starting at 0x40000000)
    auto res = map.total.try_add(0x40000000, 128 * 1024 * 1024);
    if (!res)
        return reloco::unexpected(res.error());

    // Clone the total map to the free map, then start punching holes
    map.free = map.total.try_clone().unwrap();

    // Punch out the Boot ROM / FDT region
    res = map.free.try_subtract(0x40000000, 0x10000);
    if (!res)
        return reloco::unexpected(res.error());

    // Punch out the Hypervisor Binary Footprint
    uintptr_t hv_start = reinterpret_cast<uintptr_t>(_start);
    uintptr_t hv_size = reinterpret_cast<uintptr_t>(__stack_top) - hv_start;
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

    const auto regions = bootstrap_memory_regions().unwrap();

    regions.free.iter().for_each([&](const auto &region)
                                 { MICROFMT_LOG_INFO("FREE {:#x} -- {:#x}", region.base, region.base + region.size); });

    regions.total.iter().for_each([&](const auto &region)
                                  { MICROFMT_LOG_INFO("TOTAL {:#x} -- {:#x}", region.base, region.base + region.size); });

    microvisor::g_pages = microvisor::page_array::create(regions).unwrap();

    MICROFMT_LOG_INFO("{} pages", microvisor::g_pages.size());

    microvisor::g_buddy.init(microvisor::page_view_4k::from_os_page(
                                 microvisor::g_pages.begin()),
                             microvisor::g_pages.size())
        .unwrap();

    s_buddy_alloc.emplace(microvisor::g_buddy);

    s_allocator.emplace(2 * sizeof(void *), s_buddy_alloc->ref(), 65536);

    microvisor::g_stage1_mmu.emplace(s_buddy_alloc->ref());

    microvisor::g_stage1_mmu->init().unwrap();

    MICROFMT_LOG_INFO("Init MMU");

    microvisor::enable_hypervisor_mmu();

    MICROFMT_LOG_INFO("MMU init Done");

    // Static device instances
    microvisor::devices::power_device g_power_dev;
    microvisor::devices::simple_console g_console_dev(0x09000000); // Host PL011 base

    auto my_vm = reloco::unique_ptr<microvisor::vm>::try_create(
                     1,                   // Passed to try_construct: VMID
                     s_buddy_alloc->ref() // Passed to try_construct: Page allocator
                     )
                     .unwrap();

    MICROFMT_LOG_INFO("VM {} successfully allocated!", my_vm->id());

    // Map Guest memory directly via operator->
    using guest_addr_t = reloco::phys_addr<void, reloco::guest_phys_space, uint64_t>;
    using host_addr_t = reloco::phys_addr<void, reloco::host_phys_space, uint64_t>;

    guest_addr_t gpa_ram{0x40000000};
    host_addr_t hpa_ram{0x44000000};

    uint64_t vm_ram_desc = microvisor::lpae_stage2::descriptor::make_block(
                               hpa_ram,
                               microvisor::lpae_stage2::MEMATTR_NORMAL_WB,
                               microvisor::lpae_stage2::S2AP_RW,
                               false)
                               .raw;

    my_vm->stage2().map_block_2m(gpa_ram, vm_ram_desc).unwrap();

    // Map Power Device at [0x80000000 - 0x80000FFF]
    my_vm->mmio().register_device(0x80000000, 0x1000, &g_power_dev);

    // Map Simple Console at [0x80001000 - 0x80001FFF]
    my_vm->mmio().register_device(0x80001000, 0x1000, &g_console_dev);

    // Write a tiny Guest Payload directly into the Host physical memory we assigned it.
    // We mapped guest 0x40000000 -> host 0x44000000.
    // Because Stage-1 maps the whole 128MB 1:1, we can write directly to 0x44000000!
    uint32_t *guest_ram = reinterpret_cast<uint32_t *>(0x44000000);

    // Assembly machine code
    guest_ram[0] = 0xE59F1024;  // 0x00: ldr  r1, [pc, #36]       -> 0x80000000
    guest_ram[1] = 0xE5910000;  // 0x04: ldr  r0, [r1]            -> MMIO READ (r0 = "VM01")
    guest_ram[2] = 0xE28F2024;  // 0x08: add  r2, pc, #36         -> Pointer to msg_str (0x34)
    guest_ram[3] = 0xE4D23001;  // 0x0C: ldrb r3, [r2], #1        -> print_loop start
    guest_ram[4] = 0xE3530000;  // 0x10: cmp  r3, #0
    guest_ram[5] = 0x0A000001;  // 0x14: beq  +1 (to 0x20)
    guest_ram[6] = 0xE5813004;  // 0x18: str  r3, [r1, #4]        -> MMIO WRITE (putchar)
    guest_ram[7] = 0xEAFFFFFA;  // 0x1C: b    -6 (to 0x0C)
    guest_ram[8] = 0xE59F3008;  // 0x20: ldr  r3, [pc, #8]        -> 0xCAFEBABE
    guest_ram[9] = 0xE5813000;  // 0x24: str  r3, [r1]            -> MMIO WRITE (shutdown)
    guest_ram[10] = 0xEAFFFFFE; // 0x28: b    .                   -> Infinite loop safety

    // Literal pool & Data (aligned)
    guest_ram[11] = 0x80000000; // 0x2C: MMIO Base address
    guest_ram[12] = 0xCAFEBABE; // 0x30: Magic shutdown code
    guest_ram[13] = 0x6C6C6548; // 0x34: "Hell"
    guest_ram[14] = 0x4D56206F; // 0x38: "o VM"
    guest_ram[15] = 0x000A0D21; // 0x3C: "!\r\n\0"

    // Flush D-Cache to Point of Unification and invalidate I-Cache
    microvisor::clear_cache(guest_ram, guest_ram + 16);

    // Configure VCPU to boot at the base of RAM in Supervisor mode
    my_vm->vcpu().pc = gpa_ram.value;
    my_vm->vcpu().cpsr = 0x000001D3; // SVC mode (0x13) + IRQ/FIQ disabled

    MICROFMT_LOG_INFO("Entering VM {}...", my_vm->id());

    bool vm_running = true;
    while (vm_running)
    {
        // Enter Guest
        my_vm->run();

        // Dispatch the Exit
        microvisor::trap::exit_status status = microvisor::trap::dispatch_exit(*my_vm);

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