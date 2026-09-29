// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

/** @file sysregs.hpp
 * @brief Save/restore of the guest's (non-secure PL1/EL1) architectural
 * system registers across world switches, with a lazy skip-if-unchanged
 * optimization.
 *
 * The raw per-register CP15 accessors (`guest_get_*`/`guest_set_*`) live in
 * src/sysregs.S, matching the split used for the virtual timer
 * (timer.S/timer.hpp): assembly only ever does the dumb mrc/mcr, and all
 * policy -- what to save, when, and the lazy-switch bookkeeping below --
 * lives here in C++.
 *
 * Unlike the Stage-2/HCR/VTTBR state (reprogrammed on every `vm::run()`
 * regardless, since it's cheap and always VM-specific) or the virtual timer
 * (CNTV, unconditionally saved/restored every switch since it's only a
 * handful of registers), the guest's EL1 system registers are comparatively
 * numerous, so this tracks which `vcpu_context` currently owns the *live*
 * hardware state (`g_hw_owner`) and skips the reload entirely when the same
 * vCPU is scheduled back-to-back -- the common case whenever only one VM is
 * actually running.
 */

#include <cstdint>
#include "microvisor/vcpu.hpp"

extern "C"
{
    uint32_t guest_get_sctlr();
    void guest_set_sctlr(uint32_t val);
    uint32_t guest_get_actlr();
    void guest_set_actlr(uint32_t val);
    uint32_t guest_get_cpacr();
    void guest_set_cpacr(uint32_t val);
    uint32_t guest_get_ttbr0();
    void guest_set_ttbr0(uint32_t val);
    uint32_t guest_get_ttbr1();
    void guest_set_ttbr1(uint32_t val);
    uint32_t guest_get_ttbcr();
    void guest_set_ttbcr(uint32_t val);
    uint32_t guest_get_dacr();
    void guest_set_dacr(uint32_t val);
    uint32_t guest_get_mair0();
    void guest_set_mair0(uint32_t val);
    uint32_t guest_get_mair1();
    void guest_set_mair1(uint32_t val);
    uint32_t guest_get_amair0();
    void guest_set_amair0(uint32_t val);
    uint32_t guest_get_amair1();
    void guest_set_amair1(uint32_t val);
    uint32_t guest_get_vbar();
    void guest_set_vbar(uint32_t val);
    uint32_t guest_get_contextidr();
    void guest_set_contextidr(uint32_t val);
    uint32_t guest_get_tpidrurw();
    void guest_set_tpidrurw(uint32_t val);
    uint32_t guest_get_tpidruro();
    void guest_set_tpidruro(uint32_t val);
    uint32_t guest_get_tpidrprw();
    void guest_set_tpidrprw(uint32_t val);
}

namespace microvisor::sysregs
{

    namespace detail
    {
        /**
         * @brief Reads every tracked guest system register out of hardware
         * into @p regs.
         */
        inline void save_to(guest_sysregs &regs) noexcept
        {
            regs.sctlr = guest_get_sctlr();
            regs.actlr = guest_get_actlr();
            regs.cpacr = guest_get_cpacr();
            regs.ttbr0 = guest_get_ttbr0();
            regs.ttbr1 = guest_get_ttbr1();
            regs.ttbcr = guest_get_ttbcr();
            regs.dacr = guest_get_dacr();
            regs.mair0 = guest_get_mair0();
            regs.mair1 = guest_get_mair1();
            regs.amair0 = guest_get_amair0();
            regs.amair1 = guest_get_amair1();
            regs.vbar = guest_get_vbar();
            regs.contextidr = guest_get_contextidr();
            regs.tpidrurw = guest_get_tpidrurw();
            regs.tpidruro = guest_get_tpidruro();
            regs.tpidrprw = guest_get_tpidrprw();
        }

        /**
         * @brief Writes every tracked guest system register in @p regs into
         * hardware.
         */
        inline void load_from(const guest_sysregs &regs) noexcept
        {
            guest_set_sctlr(regs.sctlr);
            guest_set_actlr(regs.actlr);
            guest_set_cpacr(regs.cpacr);
            guest_set_ttbr0(regs.ttbr0);
            guest_set_ttbr1(regs.ttbr1);
            guest_set_ttbcr(regs.ttbcr);
            guest_set_dacr(regs.dacr);
            guest_set_mair0(regs.mair0);
            guest_set_mair1(regs.mair1);
            guest_set_amair0(regs.amair0);
            guest_set_amair1(regs.amair1);
            guest_set_vbar(regs.vbar);
            guest_set_contextidr(regs.contextidr);
            guest_set_tpidrurw(regs.tpidrurw);
            guest_set_tpidruro(regs.tpidruro);
            guest_set_tpidrprw(regs.tpidrprw);
        }

        // The vcpu_context whose system registers are currently live in
        // hardware, or nullptr if none has been loaded yet this boot.
        inline vcpu_context *g_hw_owner = nullptr;
    } // namespace detail

    /**
     * @brief Ensures @p vcpu's system-register state is live in hardware
     * before VM entry.
     *
     * Lazy: if @p vcpu was already the last vCPU scheduled (and thus no
     * other vCPU's registers have been loaded since), hardware already
     * holds the correct values and this is just a pointer comparison --
     * zero mrc/mcr traffic in the common single-VM case. Only when control
     * is actually handed to a *different* vCPU do we flush the previous
     * owner's live state back to its `vcpu_context` and load the new one.
     */
    inline void restore_guest_sysregs(vcpu_context &vcpu) noexcept
    {
        if (detail::g_hw_owner == &vcpu)
            return;

        if (detail::g_hw_owner != nullptr)
            detail::save_to(detail::g_hw_owner->sysregs);

        detail::load_from(vcpu.sysregs);
        detail::g_hw_owner = &vcpu;
    }

    /**
     * @brief Marks the end of @p vcpu's execution slice.
     *
     * Deliberately a no-op: with the lazy scheme above, hardware is left
     * holding @p vcpu's state for as long as it keeps being the vCPU that
     * gets scheduled, and is only actually read back on demand, right
     * before a *different* vCPU's @ref restore_guest_sysregs would
     * otherwise clobber it. Kept as a named call (mirroring
     * `timer::save_guest_timer`) so call sites read symmetrically and the
     * laziness is documented at the point where a naive implementation
     * would have done the write-back.
     */
    inline void save_guest_sysregs(vcpu_context &vcpu) noexcept { (void)vcpu; }

    /**
     * @brief Drops hardware ownership tracking for @p vcpu without
     * flushing anything back, for use when @p vcpu is being destroyed.
     *
     * Safe to skip the flush here: the `vcpu_context` is going away either
     * way, and clearing `g_hw_owner` just guarantees the next
     * @ref restore_guest_sysregs call (for whatever vCPU runs next) won't
     * compare against a dangling pointer and will properly reload from
     * scratch.
     */
    inline void invalidate_owner(vcpu_context &vcpu) noexcept
    {
        if (detail::g_hw_owner == &vcpu)
            detail::g_hw_owner = nullptr;
    }

} // namespace microvisor::sysregs
