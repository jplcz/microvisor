// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

.syntax unified
.arch armv7-a
.arch_extension virt
.arm

.global hyp_vector_table
.global hyp_set_hvbar
.global hyp_get_cpsr

// -----------------------------------------------------------------------------
// Hyp Vector Table
// Must be aligned to 32 bytes (2^5). 
// -----------------------------------------------------------------------------
.align 5
.section .text.hyp_vector_table, "ax"
.type hyp_vector_table, %function
hyp_vector_table:
    b .              // 0x00: Reset (Never happens in Hyp mode)
    b hyp_trap_undef // 0x04: Undefined Instruction
    b hyp_trap_hvc   // 0x08: Hypervisor Call (HVC / SMC)
    b hyp_trap_pabt  // 0x0C: Prefetch Abort
    b hyp_trap_dabt  // 0x10: Data Abort
    b hyp_trap_hyp   // 0x14: Hyp Trap
    b hyp_trap_irq   // 0x18: IRQ
    b hyp_trap_fiq   // 0x1C: FIQ

    .size hyp_vector_table, . - hyp_vector_table

// -----------------------------------------------------------------------------
// void hyp_set_hvbar(uint32_t addr);
// Sets the Hypervisor Vector Base Address Register
// -----------------------------------------------------------------------------
.section .text.hyp_set_hvbar, "ax"
hyp_set_hvbar:
    .type hyp_set_hvbar, %function
    // Write r0 to HVBAR (CP15, OP1=4, CRn=12, CRm=0, OP2=0)
    mcr p15, 4, r0, c12, c0, 0
    isb
    bx lr
    .size hyp_set_hvbar, . - hyp_set_hvbar

// -----------------------------------------------------------------------------
// uint32_t hyp_get_cpsr();
// Returns the Current Program Status Register
// -----------------------------------------------------------------------------
.section .text.hyp_get_cpsr, "ax"
hyp_get_cpsr:
    .type hyp_get_cpsr, %function
    mrs r0, cpsr
    bx lr
    .size hyp_get_cpsr, . - hyp_get_cpsr

.text

// -----------------------------------------------------------------------------
// Vector Trampoline Macro
// -----------------------------------------------------------------------------
.macro TRAP_ENTRY vector_offset
    // Free up r0-r3 for immediate working use
    push {r0-r3}
    
    // Retrieve our vcpu_context pointer from HTPIDR into r0
    mrc p15, 4, r0, c13, c0, 2
    isb

    // Tag the vcpu->exit_vector (offset 0x44) with the hardware vector
    mov r1, #\vector_offset
    str r1, [r0, #0x44]
    
    // Jump to the common state-saving routine
    b hyp_common_exit
.endm

hyp_trap_undef: TRAP_ENTRY 0x04
hyp_trap_hvc:   TRAP_ENTRY 0x08
hyp_trap_pabt:  TRAP_ENTRY 0x0C
hyp_trap_dabt:  TRAP_ENTRY 0x10
hyp_trap_hyp:   TRAP_ENTRY 0x14
hyp_trap_irq:   TRAP_ENTRY 0x18
hyp_trap_fiq:   TRAP_ENTRY 0x1C


// -----------------------------------------------------------------------------
// Common VM Exit Handler
// -----------------------------------------------------------------------------
.type hyp_common_exit, %function
hyp_common_exit:
    // We arrive here from a TRAP_ENTRY stub. 
    // Guest r0-r3 are on the stack. r0 holds the vcpu pointer.

    // Save guest r4-r12 directly into the vcpu_context struct
    add r1, r0, #16         // offset to vcpu->r[4]
    stmia r1, {r4-r12}

    // Pop guest r0-r3 from the stack and save them into the struct
    pop {r4-r7}             
    stmia r0, {r4-r7}       

    // Save the Guest PC and CPSR
    mrs r2, elr_hyp
    str r2, [r0, #0x3C]     // vcpu->pc
    
    mrs r3, spsr
    str r3, [r0, #0x40]     // vcpu->cpsr

    // Restore the Host C++ state
    pop {r4-r12, lr}

    // Return to the C++ while loop
    bx lr
.size hyp_common_exit, . - hyp_common_exit