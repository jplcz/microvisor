// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

// Raw CP15 accessors for the guest's (non-secure PL1/EL1) architectural
// system registers. From Hyp mode these encodings (opc1=0) address the
// guest's own banked copies directly -- distinct from the Hyp-mode-only
// registers (opc1=4, e.g. HSCTLR/HCR in mmu.S) -- so no extra banking
// tricks are needed here, just the plain mrc/mcr pair per register.
//
// These are intentionally dumb read/write primitives with no save/restore
// policy attached: the orchestration (what to save, when, and the lazy
// skip-if-unchanged optimization) lives in C++, in
// include/microvisor/sysregs.hpp, per project convention (see timer.S/
// timer.hpp for the same split).

.syntax unified
.arch armv7-a
.arch_extension virt
.arm

.global guest_get_sctlr
.global guest_set_sctlr
.global guest_get_actlr
.global guest_set_actlr
.global guest_get_cpacr
.global guest_set_cpacr
.global guest_get_ttbr0
.global guest_set_ttbr0
.global guest_get_ttbr1
.global guest_set_ttbr1
.global guest_get_ttbcr
.global guest_set_ttbcr
.global guest_get_dacr
.global guest_set_dacr
.global guest_get_mair0
.global guest_set_mair0
.global guest_get_mair1
.global guest_set_mair1
.global guest_get_amair0
.global guest_set_amair0
.global guest_get_amair1
.global guest_set_amair1
.global guest_get_vbar
.global guest_set_vbar
.global guest_get_contextidr
.global guest_set_contextidr
.global guest_get_tpidrurw
.global guest_set_tpidrurw
.global guest_get_tpidruro
.global guest_set_tpidruro
.global guest_get_tpidrprw
.global guest_set_tpidrprw

// -----------------------------------------------------------------------------
// SCTLR (System Control Register) -- CP15, c1, c0, opc1=0, opc2=0
// -----------------------------------------------------------------------------
.type guest_get_sctlr, %function
guest_get_sctlr:
    mrc p15, 0, r0, c1, c0, 0
    bx lr
.size guest_get_sctlr, . - guest_get_sctlr

.type guest_set_sctlr, %function
guest_set_sctlr:
    mcr p15, 0, r0, c1, c0, 0
    isb
    bx lr
.size guest_set_sctlr, . - guest_set_sctlr

// -----------------------------------------------------------------------------
// ACTLR (Auxiliary Control Register) -- CP15, c1, c0, opc1=0, opc2=1
// -----------------------------------------------------------------------------
.type guest_get_actlr, %function
guest_get_actlr:
    mrc p15, 0, r0, c1, c0, 1
    bx lr
.size guest_get_actlr, . - guest_get_actlr

.type guest_set_actlr, %function
guest_set_actlr:
    mcr p15, 0, r0, c1, c0, 1
    isb
    bx lr
.size guest_set_actlr, . - guest_set_actlr

// -----------------------------------------------------------------------------
// CPACR (Coprocessor Access Control Register) -- CP15, c1, c0, opc1=0, opc2=2
// -----------------------------------------------------------------------------
.type guest_get_cpacr, %function
guest_get_cpacr:
    mrc p15, 0, r0, c1, c0, 2
    bx lr
.size guest_get_cpacr, . - guest_get_cpacr

.type guest_set_cpacr, %function
guest_set_cpacr:
    mcr p15, 0, r0, c1, c0, 2
    isb
    bx lr
.size guest_set_cpacr, . - guest_set_cpacr

// -----------------------------------------------------------------------------
// TTBR0 / TTBR1 (Translation Table Base Registers, short-descriptor form)
// CP15, c2, c0, opc1=0, opc2={0,1}
// -----------------------------------------------------------------------------
.type guest_get_ttbr0, %function
guest_get_ttbr0:
    mrc p15, 0, r0, c2, c0, 0
    bx lr
.size guest_get_ttbr0, . - guest_get_ttbr0

.type guest_set_ttbr0, %function
guest_set_ttbr0:
    mcr p15, 0, r0, c2, c0, 0
    isb
    bx lr
.size guest_set_ttbr0, . - guest_set_ttbr0

.type guest_get_ttbr1, %function
guest_get_ttbr1:
    mrc p15, 0, r0, c2, c0, 1
    bx lr
.size guest_get_ttbr1, . - guest_get_ttbr1

.type guest_set_ttbr1, %function
guest_set_ttbr1:
    mcr p15, 0, r0, c2, c0, 1
    isb
    bx lr
.size guest_set_ttbr1, . - guest_set_ttbr1

// -----------------------------------------------------------------------------
// TTBCR (Translation Table Base Control Register) -- CP15, c2, c0, opc1=0, opc2=2
// -----------------------------------------------------------------------------
.type guest_get_ttbcr, %function
guest_get_ttbcr:
    mrc p15, 0, r0, c2, c0, 2
    bx lr
.size guest_get_ttbcr, . - guest_get_ttbcr

.type guest_set_ttbcr, %function
guest_set_ttbcr:
    mcr p15, 0, r0, c2, c0, 2
    isb
    bx lr
.size guest_set_ttbcr, . - guest_set_ttbcr

// -----------------------------------------------------------------------------
// DACR (Domain Access Control Register) -- CP15, c3, c0, opc1=0, opc2=0
// -----------------------------------------------------------------------------
.type guest_get_dacr, %function
guest_get_dacr:
    mrc p15, 0, r0, c3, c0, 0
    bx lr
.size guest_get_dacr, . - guest_get_dacr

.type guest_set_dacr, %function
guest_set_dacr:
    mcr p15, 0, r0, c3, c0, 0
    isb
    bx lr
.size guest_set_dacr, . - guest_set_dacr

// -----------------------------------------------------------------------------
// MAIR0/MAIR1 (aka PRRR/NMRR) -- CP15, c10, c2, opc1=0, opc2={0,1}
// -----------------------------------------------------------------------------
.type guest_get_mair0, %function
guest_get_mair0:
    mrc p15, 0, r0, c10, c2, 0
    bx lr
.size guest_get_mair0, . - guest_get_mair0

.type guest_set_mair0, %function
guest_set_mair0:
    mcr p15, 0, r0, c10, c2, 0
    isb
    bx lr
.size guest_set_mair0, . - guest_set_mair0

.type guest_get_mair1, %function
guest_get_mair1:
    mrc p15, 0, r0, c10, c2, 1
    bx lr
.size guest_get_mair1, . - guest_get_mair1

.type guest_set_mair1, %function
guest_set_mair1:
    mcr p15, 0, r0, c10, c2, 1
    isb
    bx lr
.size guest_set_mair1, . - guest_set_mair1

// -----------------------------------------------------------------------------
// AMAIR0/AMAIR1 (Auxiliary Memory Attribute Indirection) -- CP15, c10, c3, opc1=0, opc2={0,1}
// -----------------------------------------------------------------------------
.type guest_get_amair0, %function
guest_get_amair0:
    mrc p15, 0, r0, c10, c3, 0
    bx lr
.size guest_get_amair0, . - guest_get_amair0

.type guest_set_amair0, %function
guest_set_amair0:
    mcr p15, 0, r0, c10, c3, 0
    isb
    bx lr
.size guest_set_amair0, . - guest_set_amair0

.type guest_get_amair1, %function
guest_get_amair1:
    mrc p15, 0, r0, c10, c3, 1
    bx lr
.size guest_get_amair1, . - guest_get_amair1

.type guest_set_amair1, %function
guest_set_amair1:
    mcr p15, 0, r0, c10, c3, 1
    isb
    bx lr
.size guest_set_amair1, . - guest_set_amair1

// -----------------------------------------------------------------------------
// VBAR (Vector Base Address Register) -- CP15, c12, c0, opc1=0, opc2=0
// -----------------------------------------------------------------------------
.type guest_get_vbar, %function
guest_get_vbar:
    mrc p15, 0, r0, c12, c0, 0
    bx lr
.size guest_get_vbar, . - guest_get_vbar

.type guest_set_vbar, %function
guest_set_vbar:
    mcr p15, 0, r0, c12, c0, 0
    isb
    bx lr
.size guest_set_vbar, . - guest_set_vbar

// -----------------------------------------------------------------------------
// CONTEXTIDR (Context ID / ASID Register) -- CP15, c13, c0, opc1=0, opc2=1
// -----------------------------------------------------------------------------
.type guest_get_contextidr, %function
guest_get_contextidr:
    mrc p15, 0, r0, c13, c0, 1
    bx lr
.size guest_get_contextidr, . - guest_get_contextidr

.type guest_set_contextidr, %function
guest_set_contextidr:
    mcr p15, 0, r0, c13, c0, 1
    isb
    bx lr
.size guest_set_contextidr, . - guest_set_contextidr

// -----------------------------------------------------------------------------
// TPIDRURW/TPIDRURO/TPIDRPRW (Thread ID registers) -- CP15, c13, c0, opc1=0, opc2={2,3,4}
// -----------------------------------------------------------------------------
.type guest_get_tpidrurw, %function
guest_get_tpidrurw:
    mrc p15, 0, r0, c13, c0, 2
    bx lr
.size guest_get_tpidrurw, . - guest_get_tpidrurw

.type guest_set_tpidrurw, %function
guest_set_tpidrurw:
    mcr p15, 0, r0, c13, c0, 2
    isb
    bx lr
.size guest_set_tpidrurw, . - guest_set_tpidrurw

.type guest_get_tpidruro, %function
guest_get_tpidruro:
    mrc p15, 0, r0, c13, c0, 3
    bx lr
.size guest_get_tpidruro, . - guest_get_tpidruro

.type guest_set_tpidruro, %function
guest_set_tpidruro:
    mcr p15, 0, r0, c13, c0, 3
    isb
    bx lr
.size guest_set_tpidruro, . - guest_set_tpidruro

.type guest_get_tpidrprw, %function
guest_get_tpidrprw:
    mrc p15, 0, r0, c13, c0, 4
    bx lr
.size guest_get_tpidrprw, . - guest_get_tpidrprw

.type guest_set_tpidrprw, %function
guest_set_tpidrprw:
    mcr p15, 0, r0, c13, c0, 4
    isb
    bx lr
.size guest_set_tpidrprw, . - guest_set_tpidrprw
