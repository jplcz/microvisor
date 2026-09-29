// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

.syntax unified
.arch armv7-a
.arm

.global hyp_get_vtcr
.global hyp_set_vtcr
.global hyp_set_vttbr
.global hyp_set_htcr
.global hyp_set_httbr
.global hyp_set_hmair0
.global hyp_get_hsctlr
.global hyp_set_hsctlr
.global hyp_get_hcr
.global hyp_set_hcr
.global hyp_get_hsr
.global hyp_get_hdfar
.global hyp_get_hifar

// -----------------------------------------------------------------------------
// VTCR (Virtualization Translation Control Register)
// CP15, CRn=2, CRm=1, opc1=4, opc2=2
// -----------------------------------------------------------------------------
.type hyp_get_vtcr, %function
hyp_get_vtcr:
    mrc p15, 4, r0, c2, c1, 2
    bx lr
.size hyp_get_vtcr, . - hyp_get_vtcr

.type hyp_set_vtcr, %function
hyp_set_vtcr:
    mcr p15, 4, r0, c2, c1, 2
    isb
    bx lr
.size hyp_set_vtcr, . - hyp_set_vtcr

// -----------------------------------------------------------------------------
// VTTBR (Virtualization Translation Table Base Register)
// 64-bit Register: CP15, CRm=2, opc1=6
// -----------------------------------------------------------------------------
.type hyp_set_vttbr, %function
hyp_set_vttbr:
    // r0 contains the lower 32 bits (Base Address + VMID)
    // r1 contains the upper 32 bits (Extended Address)
    mcrr p15, 6, r0, r1, c2
    isb
    bx lr
.size hyp_set_vttbr, . - hyp_set_vttbr

// -----------------------------------------------------------------------------
// Hypervisor Stage-1 MMU Registers (EL2)
// -----------------------------------------------------------------------------

.type hyp_set_htcr, %function
hyp_set_htcr:
    mcr p15, 4, r0, c2, c0, 2
    isb
    bx lr
.size hyp_set_htcr, . - hyp_set_htcr

.type hyp_set_httbr, %function
hyp_set_httbr:
    // HTTBR is a 64-bit register: r0=low, r1=high
    mcrr p15, 4, r0, r1, c2
    isb
    bx lr
.size hyp_set_httbr, . - hyp_set_httbr

.type hyp_set_hmair0, %function
hyp_set_hmair0:
    mcr p15, 4, r0, c10, c2, 0
    isb
    bx lr
.size hyp_set_hmair0, . - hyp_set_hmair0

.type hyp_get_hsctlr, %function
hyp_get_hsctlr:
    mrc p15, 4, r0, c1, c0, 0
    bx lr
.size hyp_get_hsctlr, . - hyp_get_hsctlr

.type hyp_set_hsctlr, %function
hyp_set_hsctlr:
    mcr p15, 4, r0, c1, c0, 0
    dsb
    isb
    bx lr
.size hyp_set_hsctlr, . - hyp_set_hsctlr

.type hyp_get_hcr, %function
hyp_get_hcr:
    mrc p15, 4, r0, c1, c1, 0
    bx lr
.size hyp_get_hcr, . - hyp_get_hcr

.type hyp_set_hcr, %function
hyp_set_hcr:
    mcr p15, 4, r0, c1, c1, 0
    isb
    bx lr
.size hyp_set_hcr, . - hyp_set_hcr

// Read Hyp Syndrome Register
.type hyp_get_hsr, %function
hyp_get_hsr:
    mrc p15, 4, r0, c5, c2, 0
    bx lr
.size hyp_get_hsr, . - hyp_get_hsr

// Read Hyp Data Fault Address Register (Where a data abort happened)
.type hyp_get_hdfar, %function
hyp_get_hdfar:
    mrc p15, 4, r0, c6, c0, 0
    bx lr
.size hyp_get_hdfar, . - hyp_get_hdfar

// Read Hyp Instruction Fault Address Register (Where a prefetch abort happened)
.type hyp_get_hifar, %function
hyp_get_hifar:
    mrc p15, 4, r0, c6, c0, 2
    bx lr
.size hyp_get_hifar, . - hyp_get_hifar

.global hyp_get_hpfar
.type hyp_get_hpfar, %function
hyp_get_hpfar:
    // Read Hyp IPA Fault Address Register
    // CP15 c6, Op1=4, c0, Op2=4
    mrc p15, 4, r0, c6, c0, 4
    bx lr
.size hyp_get_hpfar, . - hyp_get_hpfar
