// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include "microvisor/vcpu.hpp"

extern "C"
{
    uint32_t timer_get_cntfrq();
    uint32_t timer_get_cnthctl();
    void timer_set_cnthctl(uint32_t val);

    void timer_set_cnthp_tval(uint32_t val);
    void timer_set_cnthp_ctl(uint32_t val);
    uint32_t timer_get_cnthp_ctl();

    uint64_t timer_get_cntvoff();
    void timer_set_cntvoff(uint32_t low, uint32_t high);

    uint32_t timer_get_cntv_ctl();
    void timer_set_cntv_ctl(uint32_t val);
    uint64_t timer_get_cntv_cval();
    void timer_set_cntv_cval(uint32_t low, uint32_t high);

    uint64_t timer_get_cntpct();
}

namespace microvisor::timer
{

    // Control Register Flags (CNTHP_CTL, CNTV_CTL)
    constexpr uint32_t CTL_ENABLE = (1 << 0);
    constexpr uint32_t CTL_IMASK = (1 << 1);
    constexpr uint32_t CTL_ISTATUS = (1 << 2);

    // CNTHCTL Bits
    constexpr uint32_t CNTHCTL_PL1PCTEN = (1 << 0); // Allow guest to read CNTPCT
    constexpr uint32_t CNTHCTL_PL1PCEN = (1 << 1);  // Allow guest access to Physical Timer

    inline void init() noexcept
    {
        // Allow guest direct access to its own virtual timer (CNTV),
        // but trap guest access to physical timers (CNTP) so it cannot tamper with host clocks.
        uint32_t cnthctl = timer_get_cnthctl();
        cnthctl &= ~CNTHCTL_PL1PCEN; // Trap CNTP accesses to EL2
        cnthctl |= CNTHCTL_PL1PCTEN; // Allow guest reading physical counter if needed
        timer_set_cnthctl(cnthctl);
    }

    /**
     * @brief Arms the Hyp Physical Timer for the next vCPU execution quantum.
     * @param quantum_ms Duration in milliseconds.
     */
    inline void arm_preemption_timer(uint32_t quantum_ms) noexcept
    {
        uint32_t freq = timer_get_cntfrq();
        uint32_t ticks = static_cast<uint32_t>((static_cast<uint64_t>(freq) * quantum_ms) / 1000ULL);

        // TVAL programs an internal down-counter relative to the current physical counter
        timer_set_cnthp_tval(ticks);
        // Enable timer, unmask interrupt
        timer_set_cnthp_ctl(CTL_ENABLE);
    }

    inline void disarm_preemption_timer() noexcept
    {
        // Disable timer to stop asserting PPI 26
        timer_set_cnthp_ctl(0);
    }

    /**
     * @brief Restores the guest's virtual timer context before VM entry.
     */
    inline void restore_guest_timer(vcpu_context &vcpu) noexcept
    {
        // If isolating stolen time, advance CNTVOFF by the duration the vCPU was paused:
        if (vcpu.last_desched != 0)
        {
            uint64_t now = timer_get_cntpct();
            vcpu.cntvoff += (now - vcpu.last_desched);
        }

        // Apply virtual offset
        timer_set_cntvoff(static_cast<uint32_t>(vcpu.cntvoff & 0xFFFFFFFF),
                          static_cast<uint32_t>(vcpu.cntvoff >> 32));

        // Restore guest comparator and control register
        timer_set_cntv_cval(static_cast<uint32_t>(vcpu.cntv_cval & 0xFFFFFFFF),
                            static_cast<uint32_t>(vcpu.cntv_cval >> 32));
        timer_set_cntv_ctl(vcpu.cntv_ctl);
    }

    /**
     * @brief Saves the guest's virtual timer state upon VM exit.
     */
    inline void save_guest_timer(vcpu_context &vcpu) noexcept
    {
        // Save current control and comparison threshold
        vcpu.cntv_ctl = timer_get_cntv_ctl();
        vcpu.cntv_cval = timer_get_cntv_cval();

        // Mask virtual timer in hardware to avoid spurious triggers while in EL2
        timer_set_cntv_ctl(vcpu.cntv_ctl | CTL_IMASK);

        vcpu.last_desched = timer_get_cntpct();
    }

} // namespace microvisor::timer