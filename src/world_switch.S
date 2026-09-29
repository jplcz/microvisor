// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

.syntax unified
.arch armv7-a
.arch_extension virt
.arm

.global hyp_enter_vm

// -----------------------------------------------------------------------------
// void hyp_enter_vm(vcpu_context* vcpu, uint32_t vttbr_low, uint32_t vttbr_high);
// r0 = vcpu
// r1 = vttbr_low
// r2 = vttbr_high
// -----------------------------------------------------------------------------
.type hyp_enter_vm, %function
hyp_enter_vm:
    // Save Host state (callee-saved registers) on the Hypervisor stack
    push {r4-r12, lr}

    // Stash the vcpu_context pointer in HTPIDR (Hyp Software Thread ID Register)
    // We do this so the exit handler knows exactly where to save the guest state later.
    mcr p15, 4, r0, c13, c0, 2
    isb

    // Activate the VM's Stage-2 Page Tables (VTTBR)
    mcrr p15, 6, r1, r2, c2
    isb

    // Set the Exception Return State (Where and How the guest boots)
    ldr r3, [r0, #0x40]     // Load vcpu->cpsr
    msr spsr_cxsf, r3       // Write to SPSR_hyp
    
    ldr r4, [r0, #0x3C]     // Load vcpu->pc
    msr elr_hyp, r4         // Write to ELR_hyp

    // Load Guest general-purpose registers (r0-r12)
    // ldm overwrites r0 at the very end, placing guest r0 into the CPU.
    ldm r0, {r0-r12}

    // Enter the Virtual Machine!
    eret
.size hyp_enter_vm, . - hyp_enter_vm
