// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

// Raw accessors for the guest's *other-mode* banked SP/LR (and FIQ's
// private R8-R12) -- general-purpose, not system, registers, so out of
// scope for structo::arch::arm::vm_banked_spsrs (structo/arch/arm/
// hyp_vm_regs.hpp), which now covers the banked SPSR_svc/abt/und/irq/fiq
// registers previously hand-rolled here.
//
// hyp_common_exit (exceptions.S) only ever saves/restores the SP/LR of
// whichever mode the guest happened to be in at trap time (plus the shared
// r0-r12). SP_usr/LR_usr/SP_svc/LR_svc/SP_abt/LR_abt/SP_und/LR_und/
// SP_irq/LR_irq/SP_fiq/LR_fiq/R8_fiq-R12_fiq are separate physical
// registers that are *not* touched by that path at all -- they are
// ordinary hardware state, shared by whichever VM happens to be
// scheduled, exactly like the CP15 system registers covered by
// structo::arch::arm::vm_guest_state. Without saving/restoring them
// per-VM, one VM's banked SVC/UND/IRQ/FIQ register values silently bleed
// into another VM the moment it switches into (or already occupies) one
// of those modes.
//
// These are ARMv7 Virtualization Extensions "banked register transfer"
// MRS/MSR forms (ARM DDI0406C B9.3): from Hyp mode they read/write another
// mode's banked register *without* actually switching to that mode.
//
// As with timer.S, this file is deliberately dumb: no policy, just one
// mrs/msr pair per register. The save/restore/lazy-switch logic lives in
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

.global guest_get_sp_abt
.global guest_set_sp_abt
.global guest_get_lr_abt
.global guest_set_lr_abt

.global guest_get_sp_und
.global guest_set_sp_und
.global guest_get_lr_und
.global guest_set_lr_und

.global guest_get_sp_irq
.global guest_set_sp_irq
.global guest_get_lr_irq
.global guest_set_lr_irq

.global guest_get_sp_fiq
.global guest_set_sp_fiq
.global guest_get_lr_fiq
.global guest_set_lr_fiq
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
.arm
.align 2
.type guest_get_sp_usr, %function
guest_get_sp_usr:
    sub     sp, sp, #4
    stm     sp, {sp}^       /* Store SP_usr to [sp], NO '!' */
    nop
    ldr     r0, [sp], #4    /* Read it into r0 and restore sp */
    bx      lr
.size guest_get_sp_usr, . - guest_get_sp_usr

.arm
.align 2
.type guest_set_sp_usr, %function
guest_set_sp_usr:
    /* Put the new value on the current (Hyp) stack */
    str     r0, [sp, #-4]!
    
    /* Load into SP_usr from [sp] WITHOUT the '!' writeback */
    ldm     sp, {sp}^
    nop                     /* Hazard bubble */
    
    /* Clean up the Hyp stack pointer manually */
    add     sp, sp, #4
    bx      lr
.size guest_set_sp_usr, . - guest_set_sp_usr

.arm
.align 2
.type guest_get_lr_usr, %function
guest_get_lr_usr:
    sub     sp, sp, #4
    stm     sp, {lr}^      /* Store LR_usr to [sp], NO '!' */
    nop
    ldr     r0, [sp], #4    /* Read it into r0 and restore sp */
    bx      lr
.size guest_get_lr_usr, . - guest_get_lr_usr

.arm
.align 2
.type guest_set_lr_usr, %function
guest_set_lr_usr:
    str     r0, [sp, #-4]!
    ldm     sp, {lr}^       /* No '!' writeback */
    nop
    add     sp, sp, #4
    bx      lr
.size guest_set_lr_usr, . - guest_set_lr_usr

// -----------------------------------------------------------------------------
// SVC banked SP/LR
// -----------------------------------------------------------------------------
.arm
.align 2
.type guest_get_sp_svc, %function
guest_get_sp_svc:
    mrs r0, sp_svc
    bx lr
.size guest_get_sp_svc, . - guest_get_sp_svc

.arm
.align 2
.type guest_set_sp_svc, %function
guest_set_sp_svc:
    msr sp_svc, r0
    bx lr
.size guest_set_sp_svc, . - guest_set_sp_svc

.arm
.align 2
.type guest_get_lr_svc, %function
guest_get_lr_svc:
    mrs r0, lr_svc
    bx lr
.size guest_get_lr_svc, . - guest_get_lr_svc

.arm
.align 2
.type guest_set_lr_svc, %function
guest_set_lr_svc:
    msr lr_svc, r0
    bx lr
.size guest_set_lr_svc, . - guest_set_lr_svc

// -----------------------------------------------------------------------------
// ABT banked SP/LR
// -----------------------------------------------------------------------------
.arm
.align 2
.type guest_get_sp_abt, %function
guest_get_sp_abt:
    mrs r0, sp_abt
    bx lr
.size guest_get_sp_abt, . - guest_get_sp_abt

.arm
.align 2
.type guest_set_sp_abt, %function
guest_set_sp_abt:
    msr sp_abt, r0
    bx lr
.size guest_set_sp_abt, . - guest_set_sp_abt

.arm
.align 2
.type guest_get_lr_abt, %function
guest_get_lr_abt:
    mrs r0, lr_abt
    bx lr
.size guest_get_lr_abt, . - guest_get_lr_abt

.arm
.align 2
.type guest_set_lr_abt, %function
guest_set_lr_abt:
    msr lr_abt, r0
    bx lr
.size guest_set_lr_abt, . - guest_set_lr_abt

// -----------------------------------------------------------------------------
// UND banked SP/LR
// -----------------------------------------------------------------------------

.arm
.align 2
.type guest_get_sp_und, %function
guest_get_sp_und:
    mrs r0, sp_und
    bx lr
.size guest_get_sp_und, . - guest_get_sp_und

.arm
.align 2
.type guest_set_sp_und, %function
guest_set_sp_und:
    msr sp_und, r0
    bx lr
.size guest_set_sp_und, . - guest_set_sp_und

.arm
.align 2
.type guest_get_lr_und, %function
guest_get_lr_und:
    mrs r0, lr_und
    bx lr
.size guest_get_lr_und, . - guest_get_lr_und

.arm
.align 2
.type guest_set_lr_und, %function
guest_set_lr_und:
    msr lr_und, r0
    bx lr
.size guest_set_lr_und, . - guest_set_lr_und

// -----------------------------------------------------------------------------
// IRQ banked SP/LR
// -----------------------------------------------------------------------------
.arm
.align 2
.type guest_get_sp_irq, %function
guest_get_sp_irq:
    mrs r0, sp_irq
    bx lr
.size guest_get_sp_irq, . - guest_get_sp_irq

.arm
.align 2
.type guest_set_sp_irq, %function
guest_set_sp_irq:
    msr sp_irq, r0
    bx lr
.size guest_set_sp_irq, . - guest_set_sp_irq

.arm
.align 2
.type guest_get_lr_irq, %function
guest_get_lr_irq:
    mrs r0, lr_irq
    bx lr
.size guest_get_lr_irq, . - guest_get_lr_irq

.arm
.align 2
.type guest_set_lr_irq, %function
guest_set_lr_irq:
    msr lr_irq, r0
    bx lr
.size guest_set_lr_irq, . - guest_set_lr_irq

// -----------------------------------------------------------------------------
// FIQ banked SP/LR + R8-R12
// -----------------------------------------------------------------------------
.arm
.align 2
.type guest_get_sp_fiq, %function
guest_get_sp_fiq:
    mrs r0, sp_fiq
    bx lr
.size guest_get_sp_fiq, . - guest_get_sp_fiq

.arm
.align 2
.type guest_set_sp_fiq, %function
guest_set_sp_fiq:
    msr sp_fiq, r0
    bx lr
.size guest_set_sp_fiq, . - guest_set_sp_fiq

.arm
.align 2
.type guest_get_lr_fiq, %function
guest_get_lr_fiq:
    mrs r0, lr_fiq
    bx lr
.size guest_get_lr_fiq, . - guest_get_lr_fiq

.arm
.align 2
.type guest_set_lr_fiq, %function
guest_set_lr_fiq:
    msr lr_fiq, r0
    bx lr
.size guest_set_lr_fiq, . - guest_set_lr_fiq

.arm
.align 2
.type guest_get_r8_fiq, %function
guest_get_r8_fiq:
    mrs r0, r8_fiq
    bx lr
.size guest_get_r8_fiq, . - guest_get_r8_fiq

.arm
.align 2
.type guest_set_r8_fiq, %function
guest_set_r8_fiq:
    msr r8_fiq, r0
    bx lr
.size guest_set_r8_fiq, . - guest_set_r8_fiq

.arm
.align 2
.type guest_get_r9_fiq, %function
guest_get_r9_fiq:
    mrs r0, r9_fiq
    bx lr
.size guest_get_r9_fiq, . - guest_get_r9_fiq

.arm
.align 2
.type guest_set_r9_fiq, %function
guest_set_r9_fiq:
    msr r9_fiq, r0
    bx lr
.size guest_set_r9_fiq, . - guest_set_r9_fiq

.arm
.align 2
.type guest_get_r10_fiq, %function
guest_get_r10_fiq:
    mrs r0, r10_fiq
    bx lr
.size guest_get_r10_fiq, . - guest_get_r10_fiq

.arm
.align 2
.type guest_set_r10_fiq, %function
guest_set_r10_fiq:
    msr r10_fiq, r0
    bx lr
.size guest_set_r10_fiq, . - guest_set_r10_fiq

.arm
.align 2
.type guest_get_r11_fiq, %function
guest_get_r11_fiq:
    mrs r0, r11_fiq
    bx lr
.size guest_get_r11_fiq, . - guest_get_r11_fiq

.arm
.align 2
.type guest_set_r11_fiq, %function
guest_set_r11_fiq:
    msr r11_fiq, r0
    bx lr
.size guest_set_r11_fiq, . - guest_set_r11_fiq

.arm
.align 2
.type guest_get_r12_fiq, %function
guest_get_r12_fiq:
    mrs r0, r12_fiq
    bx lr
.size guest_get_r12_fiq, . - guest_get_r12_fiq

.arm
.align 2
.type guest_set_r12_fiq, %function
guest_set_r12_fiq:
    msr r12_fiq, r0
    bx lr
.size guest_set_r12_fiq, . - guest_set_r12_fiq
