/*
 * FRF HAL ARM32 MMU/cache transition, primary core only.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Call only when data cache / MMU are already disabled.
 */
.syntax unified
.cpu cortex-a53
.arm
.text
.balign 4
.global frf_hal_mmu_activate
.type frf_hal_mmu_activate,%function
frf_hal_mmu_activate:
    cpsid if
    dsb sy
    isb
    /* TTBR0: 16KiB L1 translation table. Inner/outer walks non-cacheable. */
    mcr p15, 0, r0, c2, c0, 0
    mov r1, #0
    mcr p15, 0, r1, c2, c0, 2  /* TTBCR: N=0, EAE=0 */
    mov r1, #1
    mcr p15, 0, r1, c3, c0, 0  /* DACR: domain 0 client */
    mov r1, #0
    mcr p15, 0, r1, c8, c7, 0  /* Invalidate unified TLB */
    mcr p15, 0, r1, c7, c5, 0  /* Invalidate instruction cache */
    dsb sy
    isb
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #0x2000        /* V: low vectors, VBAR */
    bic r1, r1, #0x10000000    /* TRE: use descriptor attributes */
    bic r1, r1, #0x20000000    /* AFE: use legacy AP */
    orr r1, r1, #1             /* M: MMU */
    orr r1, r1, #4             /* C: D-cache */
    orr r1, r1, #0x1000        /* I: I-cache */
    mcr p15, 0, r1, c1, c0, 0
    isb
    bx lr
.size frf_hal_mmu_activate,.-frf_hal_mmu_activate
