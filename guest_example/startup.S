// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

// Minimal Guest-side boot shim, trimmed down from src/startup.S: this runs
// in the VM's (unprivileged, SVC-mode) context, so unlike the hypervisor it
// must NOT touch Hyp-only state (no HCPTR, no DTB capture) -- Stage-2 and
// the VCPU's initial CPSR are entirely the hypervisor's responsibility.

.syntax unified
.arch armv7-a
.arm

.global _start

// Symbols provided by linker.ld
.extern __stack_top
.extern __bss_start
.extern __bss_end
.extern __init_array_start
.extern __init_array_end

// The C++ entry point (see main.cpp)
.extern main

.section .text.boot, "ax"

_start:
    // Set up the boot stack; the VCPU is handed to us with SP unspecified.
    ldr sp, =__stack_top

    // Zero BSS, exactly like the hypervisor's own startup.S.
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
.L_bss_loop:
    cmp r0, r1
    bge .L_bss_done
    str r2, [r0], #4
    b .L_bss_loop
.L_bss_done:

    // Run C++ global constructors.
    ldr r4, =__init_array_start
    ldr r5, =__init_array_end
.L_init_loop:
    cmp r4, r5
    bge .L_init_done
    ldr r3, [r4], #4
    blx r3
    b .L_init_loop
.L_init_done:

    bl main

    // If main() ever returns (it shouldn't -- it asks the hypervisor to
    // halt the VM), just spin.
.L_halt:
    wfi
    b .L_halt

.globl __aeabi_unwind_cpp_pr0
__aeabi_unwind_cpp_pr0:
    .type __aeabi_unwind_cpp_pr0, %function
    b .
.globl __aeabi_unwind_cpp_pr1
__aeabi_unwind_cpp_pr1:
    .type __aeabi_unwind_cpp_pr1, %function
    b .
.globl __aeabi_unwind_cpp_pr2
__aeabi_unwind_cpp_pr2:
    .type __aeabi_unwind_cpp_pr2, %function
    b .
