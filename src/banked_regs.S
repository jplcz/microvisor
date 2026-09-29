// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

// Raw accessors for the guest's *other-mode* banked registers.
//
// hyp_common_exit (exceptions.S) only ever saves/restores the SP/LR of
// whichever mode the guest happened to be in at trap time (plus the shared
// r0-r12). SP_usr/LR_usr/SP_svc/LR_svc/SPSR_svc/SP_abt/LR_abt/SPSR_abt/
// SP_und/LR_und/SPSR_und/SP_irq/LR_irq/SPSR_irq/SP_fiq/LR_fiq/SPSR_fiq/
// R8_fiq-R12_fiq are separate physical registers that are *not* touched by
// that path at all -- they are ordinary hardware state, shared by whichever
// VM happens to be scheduled, exactly like the CP15 system registers in
// sysregs.S. Without saving/restoring them per-VM, one VM's banked SVC/UND/
// IRQ/FIQ register values silently bleed into another VM the moment it
// switches into (or already occupies) one of those modes.
//
// These are ARMv7 Virtualization Extensions "banked register transfer"
// MRS/MSR forms (ARM DDI0406C B9.3): from Hyp mode they read/write another
// mode's banked register *without* actually switching to that mode.
//
// As with sysregs.S/timer.S, this file is deliberately dumb: no policy, just
// one mrs/msr pair per register. The save/restore/lazy-switch logic lives in
// C++ (include/microvisor/sysregs.hpp).

.syntax unified
.arch armv7-a
.arch_extension virt
.arm

.global guest_get_sp_usr
.global guest_set_sp_usr
.global guest_get_lr_usr
.global guest_set_lr_usr

.global guest_get_sp_svc
.global guest_set_sp_svc
.global guest_get_lr_svc
.global guest_set_lr_svc
.global guest_get_spsr_svc
.global guest_set_spsr_svc

.global guest_get_sp_abt
.global guest_set_sp_abt
.global guest_get_lr_abt
.global guest_set_lr_abt
.global guest_get_spsr_abt
.global guest_set_spsr_abt

.global guest_get_sp_und
.global guest_set_sp_und
.global guest_get_lr_und
.global guest_set_lr_und
.global guest_get_spsr_und
.global guest_set_spsr_und

.global guest_get_sp_irq
.global guest_set_sp_irq
.global guest_get_lr_irq
.global guest_set_lr_irq
.global guest_get_spsr_irq
.global guest_set_spsr_irq

.global guest_get_sp_fiq
.global guest_set_sp_fiq
.global guest_get_lr_fiq
.global guest_set_lr_fiq
.global guest_get_spsr_fiq
.global guest_set_spsr_fiq
.global guest_get_r8_fiq
.global guest_set_r8_fiq
.global guest_get_r9_fiq
.global guest_set_r9_fiq
.global guest_get_r10_fiq
.global guest_set_r10_fiq
.global guest_get_r11_fiq
.global guest_set_r11_fiq
.global guest_get_r12_fiq
.global guest_set_r12_fiq

// -----------------------------------------------------------------------------
// USR/SYS banked SP/LR
// -----------------------------------------------------------------------------
.type guest_get_sp_usr, %function
guest_get_sp_usr:
    mrs r0, sp_usr
    bx lr
.size guest_get_sp_usr, . - guest_get_sp_usr

.type guest_set_sp_usr, %function
guest_set_sp_usr:
    msr sp_usr, r0
    bx lr
.size guest_set_sp_usr, . - guest_set_sp_usr

.type guest_get_lr_usr, %function
guest_get_lr_usr:
    mrs r0, lr_usr
    bx lr
.size guest_get_lr_usr, . - guest_get_lr_usr

.type guest_set_lr_usr, %function
guest_set_lr_usr:
    msr lr_usr, r0
    bx lr
.size guest_set_lr_usr, . - guest_set_lr_usr

// -----------------------------------------------------------------------------
// SVC banked SP/LR/SPSR
// -----------------------------------------------------------------------------
.type guest_get_sp_svc, %function
guest_get_sp_svc:
    mrs r0, sp_svc
    bx lr
.size guest_get_sp_svc, . - guest_get_sp_svc

.type guest_set_sp_svc, %function
guest_set_sp_svc:
    msr sp_svc, r0
    bx lr
.size guest_set_sp_svc, . - guest_set_sp_svc

.type guest_get_lr_svc, %function
guest_get_lr_svc:
    mrs r0, lr_svc
    bx lr
.size guest_get_lr_svc, . - guest_get_lr_svc

.type guest_set_lr_svc, %function
guest_set_lr_svc:
    msr lr_svc, r0
    bx lr
.size guest_set_lr_svc, . - guest_set_lr_svc

.type guest_get_spsr_svc, %function
guest_get_spsr_svc:
    mrs r0, spsr_svc
    bx lr
.size guest_get_spsr_svc, . - guest_get_spsr_svc

.type guest_set_spsr_svc, %function
guest_set_spsr_svc:
    msr spsr_svc, r0
    bx lr
.size guest_set_spsr_svc, . - guest_set_spsr_svc

// -----------------------------------------------------------------------------
// ABT banked SP/LR/SPSR
// -----------------------------------------------------------------------------
.type guest_get_sp_abt, %function
guest_get_sp_abt:
    mrs r0, sp_abt
    bx lr
.size guest_get_sp_abt, . - guest_get_sp_abt

.type guest_set_sp_abt, %function
guest_set_sp_abt:
    msr sp_abt, r0
    bx lr
.size guest_set_sp_abt, . - guest_set_sp_abt

.type guest_get_lr_abt, %function
guest_get_lr_abt:
    mrs r0, lr_abt
    bx lr
.size guest_get_lr_abt, . - guest_get_lr_abt

.type guest_set_lr_abt, %function
guest_set_lr_abt:
    msr lr_abt, r0
    bx lr
.size guest_set_lr_abt, . - guest_set_lr_abt

.type guest_get_spsr_abt, %function
guest_get_spsr_abt:
    mrs r0, spsr_abt
    bx lr
.size guest_get_spsr_abt, . - guest_get_spsr_abt

.type guest_set_spsr_abt, %function
guest_set_spsr_abt:
    msr spsr_abt, r0
    bx lr
.size guest_set_spsr_abt, . - guest_set_spsr_abt

// -----------------------------------------------------------------------------
// UND banked SP/LR/SPSR
// -----------------------------------------------------------------------------
.type guest_get_sp_und, %function
guest_get_sp_und:
    mrs r0, sp_und
    bx lr
.size guest_get_sp_und, . - guest_get_sp_und

.type guest_set_sp_und, %function
guest_set_sp_und:
    msr sp_und, r0
    bx lr
.size guest_set_sp_und, . - guest_set_sp_und

.type guest_get_lr_und, %function
guest_get_lr_und:
    mrs r0, lr_und
    bx lr
.size guest_get_lr_und, . - guest_get_lr_und

.type guest_set_lr_und, %function
guest_set_lr_und:
    msr lr_und, r0
    bx lr
.size guest_set_lr_und, . - guest_set_lr_und

.type guest_get_spsr_und, %function
guest_get_spsr_und:
    mrs r0, spsr_und
    bx lr
.size guest_get_spsr_und, . - guest_get_spsr_und

.type guest_set_spsr_und, %function
guest_set_spsr_und:
    msr spsr_und, r0
    bx lr
.size guest_set_spsr_und, . - guest_set_spsr_und

// -----------------------------------------------------------------------------
// IRQ banked SP/LR/SPSR
// -----------------------------------------------------------------------------
.type guest_get_sp_irq, %function
guest_get_sp_irq:
    mrs r0, sp_irq
    bx lr
.size guest_get_sp_irq, . - guest_get_sp_irq

.type guest_set_sp_irq, %function
guest_set_sp_irq:
    msr sp_irq, r0
    bx lr
.size guest_set_sp_irq, . - guest_set_sp_irq

.type guest_get_lr_irq, %function
guest_get_lr_irq:
    mrs r0, lr_irq
    bx lr
.size guest_get_lr_irq, . - guest_get_lr_irq

.type guest_set_lr_irq, %function
guest_set_lr_irq:
    msr lr_irq, r0
    bx lr
.size guest_set_lr_irq, . - guest_set_lr_irq

.type guest_get_spsr_irq, %function
guest_get_spsr_irq:
    mrs r0, spsr_irq
    bx lr
.size guest_get_spsr_irq, . - guest_get_spsr_irq

.type guest_set_spsr_irq, %function
guest_set_spsr_irq:
    msr spsr_irq, r0
    bx lr
.size guest_set_spsr_irq, . - guest_set_spsr_irq

// -----------------------------------------------------------------------------
// FIQ banked SP/LR/SPSR + R8-R12
// -----------------------------------------------------------------------------
.type guest_get_sp_fiq, %function
guest_get_sp_fiq:
    mrs r0, sp_fiq
    bx lr
.size guest_get_sp_fiq, . - guest_get_sp_fiq

.type guest_set_sp_fiq, %function
guest_set_sp_fiq:
    msr sp_fiq, r0
    bx lr
.size guest_set_sp_fiq, . - guest_set_sp_fiq

.type guest_get_lr_fiq, %function
guest_get_lr_fiq:
    mrs r0, lr_fiq
    bx lr
.size guest_get_lr_fiq, . - guest_get_lr_fiq

.type guest_set_lr_fiq, %function
guest_set_lr_fiq:
    msr lr_fiq, r0
    bx lr
.size guest_set_lr_fiq, . - guest_set_lr_fiq

.type guest_get_spsr_fiq, %function
guest_get_spsr_fiq:
    mrs r0, spsr_fiq
    bx lr
.size guest_get_spsr_fiq, . - guest_get_spsr_fiq

.type guest_set_spsr_fiq, %function
guest_set_spsr_fiq:
    msr spsr_fiq, r0
    bx lr
.size guest_set_spsr_fiq, . - guest_set_spsr_fiq

.type guest_get_r8_fiq, %function
guest_get_r8_fiq:
    mrs r0, r8_fiq
    bx lr
.size guest_get_r8_fiq, . - guest_get_r8_fiq

.type guest_set_r8_fiq, %function
guest_set_r8_fiq:
    msr r8_fiq, r0
    bx lr
.size guest_set_r8_fiq, . - guest_set_r8_fiq

.type guest_get_r9_fiq, %function
guest_get_r9_fiq:
    mrs r0, r9_fiq
    bx lr
.size guest_get_r9_fiq, . - guest_get_r9_fiq

.type guest_set_r9_fiq, %function
guest_set_r9_fiq:
    msr r9_fiq, r0
    bx lr
.size guest_set_r9_fiq, . - guest_set_r9_fiq

.type guest_get_r10_fiq, %function
guest_get_r10_fiq:
    mrs r0, r10_fiq
    bx lr
.size guest_get_r10_fiq, . - guest_get_r10_fiq

.type guest_set_r10_fiq, %function
guest_set_r10_fiq:
    msr r10_fiq, r0
    bx lr
.size guest_set_r10_fiq, . - guest_set_r10_fiq

.type guest_get_r11_fiq, %function
guest_get_r11_fiq:
    mrs r0, r11_fiq
    bx lr
.size guest_get_r11_fiq, . - guest_get_r11_fiq

.type guest_set_r11_fiq, %function
guest_set_r11_fiq:
    msr r11_fiq, r0
    bx lr
.size guest_set_r11_fiq, . - guest_set_r11_fiq

.type guest_get_r12_fiq, %function
guest_get_r12_fiq:
    mrs r0, r12_fiq
    bx lr
.size guest_get_r12_fiq, . - guest_get_r12_fiq

.type guest_set_r12_fiq, %function
guest_set_r12_fiq:
    msr r12_fiq, r0
    bx lr
.size guest_set_r12_fiq, . - guest_set_r12_fiq
