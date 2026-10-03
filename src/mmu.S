// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

// Stage-2/non-secure TLB maintenance. Every other EL2 system register this
// file used to expose by hand (VTCR, VTTBR, HTCR, HTTBR, HMAIR0, HSCTLR,
// HCR, HSR, HDFAR, HIFAR, HPFAR, VMPIDR, VPIDR) is now read/written via
// structo::arch::arm::sysreg_raw (structo/arch/arm/sysregs_generated.hpp),
// which emits the exact same mrc/mcr/mrrc/mcrr encodings inline -- see
// microvisor/mmu.hpp, microvisor/vm.hpp and microvisor/trap_handler.hpp.
// TLBI is an operation, not a register, so it has no structo sysreg
// equivalent and stays hand-rolled here.

.syntax unified
.arch armv7-a
.arm

.global hyp_tlbi_all_nsnh

// Invalidate entire Non-secure TLB (Stage-1 and Stage-2, all VMIDs), Hyp op.
// TLBIALLNSNH: CP15, opc1=4, CRn=c8, CRm=c7, opc2=4
.type hyp_tlbi_all_nsnh, %function
hyp_tlbi_all_nsnh:
    mov r0, #0
    mcr p15, 4, r0, c8, c7, 4
    dsb ish
    isb
    bx lr
.size hyp_tlbi_all_nsnh, . - hyp_tlbi_all_nsnh
