// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

/** @file sysregs.hpp
 * @brief Save/restore of the guest's (non-secure PL1/EL1) architectural
 * system registers across world switches, with a lazy skip-if-unchanged
 * optimization.
 *
 * The raw per-register CP15/banked-transfer accessors live in
 * structo::arch::arm::sysreg_raw (structo/arch/arm/sysregs_generated.hpp,
 * grouped for this exact purpose by structo/arch/arm/hyp_vm_regs.hpp's
 * `vm_guest_state`/`vm_banked_spsrs`) -- structo's assembly only ever does
 * the dumb mrc/mcr/mrs/msr, and all policy -- what to save, when, and the
 * lazy-switch bookkeeping below -- lives here in C++. The guest's banked
 * SP/LR (plus FIQ's private R8-R12) remain hand-rolled in
 * src/banked_regs.S, since those are general-purpose, not system,
 * registers and out of scope for `vm_banked_spsrs`.
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

#pragma once

#include <cstdint>
#include "microvisor/vcpu.hpp"

extern "C"
{
    // Other-mode banked SP/LR (and FIQ's private R8-R12) -- src/banked_regs.S.
    // The banked SPSRs are handled via structo::arch::arm::vm_banked_spsrs
    // instead (see guest_banked_regs in microvisor/vcpu.hpp).
    uint32_t guest_get_sp_usr();
    void guest_set_sp_usr(uint32_t val);
    uint32_t guest_get_lr_usr();
    void guest_set_lr_usr(uint32_t val);

    uint32_t guest_get_sp_svc();
    void guest_set_sp_svc(uint32_t val);
    uint32_t guest_get_lr_svc();
    void guest_set_lr_svc(uint32_t val);

    uint32_t guest_get_sp_abt();
    void guest_set_sp_abt(uint32_t val);
    uint32_t guest_get_lr_abt();
    void guest_set_lr_abt(uint32_t val);

    uint32_t guest_get_sp_und();
    void guest_set_sp_und(uint32_t val);
    uint32_t guest_get_lr_und();
    void guest_set_lr_und(uint32_t val);

    uint32_t guest_get_sp_irq();
    void guest_set_sp_irq(uint32_t val);
    uint32_t guest_get_lr_irq();
    void guest_set_lr_irq(uint32_t val);

    uint32_t guest_get_sp_fiq();
    void guest_set_sp_fiq(uint32_t val);
    uint32_t guest_get_lr_fiq();
    void guest_set_lr_fiq(uint32_t val);
    uint32_t guest_get_r8_fiq();
    void guest_set_r8_fiq(uint32_t val);
    uint32_t guest_get_r9_fiq();
    void guest_set_r9_fiq(uint32_t val);
    uint32_t guest_get_r10_fiq();
    void guest_set_r10_fiq(uint32_t val);
    uint32_t guest_get_r11_fiq();
    void guest_set_r11_fiq(uint32_t val);
    uint32_t guest_get_r12_fiq();
    void guest_set_r12_fiq(uint32_t val);
}

namespace microvisor::sysregs
{

    namespace detail
    {
        /**
         * @brief Reads every tracked guest banked (other-mode) register out
         * of hardware into @p regs.
         */
        inline void save_to(guest_banked_regs &regs) noexcept
        {
            regs.sp_usr = guest_get_sp_usr();
            // regs.lr_usr = guest_get_lr_usr();

            regs.sp_svc = guest_get_sp_svc();
            regs.lr_svc = guest_get_lr_svc();

            regs.sp_abt = guest_get_sp_abt();
            regs.lr_abt = guest_get_lr_abt();

            regs.sp_und = guest_get_sp_und();
            regs.lr_und = guest_get_lr_und();

            regs.sp_irq = guest_get_sp_irq();
            regs.lr_irq = guest_get_lr_irq();

            regs.sp_fiq = guest_get_sp_fiq();
            regs.lr_fiq = guest_get_lr_fiq();
            regs.r8_fiq = guest_get_r8_fiq();
            regs.r9_fiq = guest_get_r9_fiq();
            regs.r10_fiq = guest_get_r10_fiq();
            regs.r11_fiq = guest_get_r11_fiq();
            regs.r12_fiq = guest_get_r12_fiq();

            regs.spsrs = structo::arch::arm::vm_banked_spsrs::save();
        }

        /**
         * @brief Writes every tracked guest banked (other-mode) register in
         * @p regs into hardware.
         */
        inline void load_from(const guest_banked_regs &regs) noexcept
        {
            guest_set_sp_usr(regs.sp_usr);
            // guest_set_lr_usr(regs.lr_usr);

            guest_set_sp_svc(regs.sp_svc);
            guest_set_lr_svc(regs.lr_svc);

            guest_set_sp_abt(regs.sp_abt);
            guest_set_lr_abt(regs.lr_abt);

            guest_set_sp_und(regs.sp_und);
            guest_set_lr_und(regs.lr_und);

            guest_set_sp_irq(regs.sp_irq);
            guest_set_lr_irq(regs.lr_irq);

            guest_set_sp_fiq(regs.sp_fiq);
            guest_set_lr_fiq(regs.lr_fiq);
            guest_set_r8_fiq(regs.r8_fiq);
            guest_set_r9_fiq(regs.r9_fiq);
            guest_set_r10_fiq(regs.r10_fiq);
            guest_set_r11_fiq(regs.r11_fiq);
            guest_set_r12_fiq(regs.r12_fiq);

            regs.spsrs.restore();
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
        {
            detail::g_hw_owner->sysregs = structo::arch::arm::vm_guest_state::save();
            detail::save_to(detail::g_hw_owner->banked);
        }

        vcpu.sysregs.restore();
        detail::load_from(vcpu.banked);
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
