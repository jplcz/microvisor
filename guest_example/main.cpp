// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

// A tiny example Guest OS payload, built as its own freestanding C++23
// executable (reusing reloco/microfmt exactly like the hypervisor does)
// instead of being hand-assembled into a raw array of ARM opcodes in
// src/main.cpp. See guest_example/CMakeLists.txt for how the resulting
// flat binary gets embedded back into microvisor.elf.

#include <microfmt/microfmt.hpp>
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
} // namespace

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
    microfmt::format_to(console.as_sink(), "[VM 1] MMIO Bus Online!\r\n");

    // Ask the hypervisor to halt the VM.
    volatile auto *power_halt = reinterpret_cast<volatile uint32_t *>(POWER_REG_HALT);
    *power_halt = POWER_HALT_MAGIC;

    // The hypervisor should have stopped scheduling us by now; spin just in
    // case the halt request is ever ignored.
    for (;;)
    {
    }
}
