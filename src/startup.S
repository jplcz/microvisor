// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

.syntax unified
.arch armv7-a
.arm

// Export the entry point so the linker script finds it
.global _start

// Import symbols defined in our linker.ld
.extern __stack_top
.extern __bss_start
.extern __bss_end
.extern __init_array_start
.extern __init_array_end

// Import the C++ entry point
.extern main

// Place this code in the exact section QEMU will execute first
.section .text.boot, "ax"

_start:
    // Setup the Stack Pointer
    // QEMU drops us in with an undefined SP. We must set it immediately 
    // so we can make function calls and use local variables.
    ldr sp, =__stack_top

    // Zero the BSS Section
    // The C/C++ standard dictates that all uninitialized global variables 
    // must be zeroed out before execution begins.
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
.L_bss_loop:
    cmp r0, r1
    bge .L_bss_done
    str r2, [r0], #4   // Store 0 (r2) into address at r0, then increment r0 by 4
    b .L_bss_loop
.L_bss_done:

    // Call C++ Global Constructors
    // Walk through the .init_array section and execute every function pointer.
    // This allows global objects like `reloco::two_level_sg_codec` to initialize.
    ldr r4, =__init_array_start
    ldr r5, =__init_array_end
.L_init_loop:
    cmp r4, r5
    bge .L_init_done
    ldr r3, [r4], #4   // Load function pointer into r3, increment r4 by 4
    blx r3             // Branch with Link and Exchange (execute the constructor)
    b .L_init_loop
.L_init_done:

    // allow all coprocessors (CP0-CP13) and VFP/NEON without trapping
    mov r0, #0
    mcr p15, 4, r0, c1, c1, 2   // Write to HCPTR
    isb

    // Enter the Hypervisor
    // The C++ environment is now fully established.
    bl main

    // Catch-all Halt
    // If main() ever returns, trap the CPU in a low-power infinite loop.
.L_halt:
    wfi                // Wait For Interrupt (puts CPU to sleep)
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
