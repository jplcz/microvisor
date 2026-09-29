// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

.syntax unified
.arch armv7-a
.arch_extension virt
.arm

// -----------------------------------------------------------------------------
// Global Counter Frequency & Hyp Timer Control
// -----------------------------------------------------------------------------
.global timer_get_cntfrq
.type timer_get_cntfrq, %function
timer_get_cntfrq:
    mrc p15, 0, r0, c14, c0, 0       // CNTFRQ
    bx lr
.size timer_get_cntfrq, . - timer_get_cntfrq

.global timer_get_cnthctl
.global timer_set_cnthctl
.type timer_get_cnthctl, %function
timer_get_cnthctl:
    mrc p15, 4, r0, c14, c1, 0       // CNTHCTL
    bx lr
.size timer_get_cnthctl, . - timer_get_cnthctl

.type timer_set_cnthctl, %function
timer_set_cnthctl:
    mcr p15, 4, r0, c14, c1, 0       // CNTHCTL
    isb
    bx lr
.size timer_set_cnthctl, . - timer_set_cnthctl

// -----------------------------------------------------------------------------
// Hyp Physical Timer (CNTHP) - Used for Hypervisor Preemption
// -----------------------------------------------------------------------------
.global timer_set_cnthp_tval
.global timer_set_cnthp_ctl
.global timer_get_cnthp_ctl

.type timer_set_cnthp_tval, %function
timer_set_cnthp_tval:
    mcr p15, 4, r0, c14, c2, 0       // CNTHP_TVAL (Down-counter value)
    isb
    bx lr
.size timer_set_cnthp_tval, . - timer_set_cnthp_tval

.type timer_set_cnthp_ctl, %function
timer_set_cnthp_ctl:
    mcr p15, 4, r0, c14, c2, 1       // CNTHP_CTL
    isb
    bx lr
.size timer_set_cnthp_ctl, . - timer_set_cnthp_ctl

.type timer_get_cnthp_ctl, %function
timer_get_cnthp_ctl:
    mrc p15, 4, r0, c14, c2, 1       // CNTHP_CTL
    bx lr
.size timer_get_cnthp_ctl, . - timer_get_cnthp_ctl

// -----------------------------------------------------------------------------
// Virtual Counter Offset (CNTVOFF) - 64-bit
// -----------------------------------------------------------------------------
.global timer_get_cntvoff
.global timer_set_cntvoff

.type timer_get_cntvoff, %function
timer_get_cntvoff:
    mrrc p15, 4, r0, r1, c14         // r0 = low, r1 = high
    bx lr
.size timer_get_cntvoff, . - timer_get_cntvoff

.type timer_set_cntvoff, %function
timer_set_cntvoff:
    mcrr p15, 4, r0, r1, c14         // CNTVOFF
    isb
    bx lr
.size timer_set_cntvoff, . - timer_set_cntvoff

// -----------------------------------------------------------------------------
// Guest Virtual Timer (CNTV) State Management
// -----------------------------------------------------------------------------
.global timer_get_cntv_ctl
.global timer_set_cntv_ctl
.global timer_get_cntv_cval
.global timer_set_cntv_cval
.global timer_get_cntpct

.type timer_get_cntv_ctl, %function
timer_get_cntv_ctl:
    mrc p15, 0, r0, c14, c3, 1       // CNTV_CTL
    bx lr
.size timer_get_cntv_ctl, . - timer_get_cntv_ctl

.type timer_set_cntv_ctl, %function
timer_set_cntv_ctl:
    mcr p15, 0, r0, c14, c3, 1       // CNTV_CTL
    isb
    bx lr
.size timer_set_cntv_ctl, . - timer_set_cntv_ctl

.type timer_get_cntv_cval, %function
timer_get_cntv_cval:
    mrrc p15, 3, r0, r1, c14         // CNTV_CVAL (64-bit comparison value)
    bx lr
.size timer_get_cntv_cval, . - timer_get_cntv_cval

.type timer_set_cntv_cval, %function
timer_set_cntv_cval:
    mcrr p15, 3, r0, r1, c14         // CNTV_CVAL
    isb
    bx lr
.size timer_set_cntv_cval, . - timer_set_cntv_cval

.type timer_get_cntpct, %function
timer_get_cntpct:
    mrrc p15, 0, r0, r1, c14         // CNTPCT (Physical Counter)
    bx lr
.size timer_get_cntpct, . - timer_get_cntpct