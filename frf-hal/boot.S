/*
 * FRF HAL -- Raspberry Pi 3 ARM32 firmware entry.
 * Copyright (c) 2026 Future Retro Fusion.
 * SPDX-License-Identifier: MIT
 * Independently authored implementation; no Circle source incorporated.
 */
.syntax unified
.cpu cortex-a53
.arm

.section .text.boot, "ax", %progbits
.balign 32
.global _start
.type _start, %function
_start:
    cpsid if
    mrc p15, 0, r0, c0, c0, 5    /* MPIDR: low bits = Pi 3 core index */
    and r0, r0, #3
    cmp r0, #0
    bne frf_secondary_park

    /* Legacy Pi 3 firmware may deliver an AArch32 kernel in HYP mode.
       A normal CPS mode change cannot leave HYP; use ERET to PL1 SVC. */
    mrs r0, cpsr
    and r1, r0, #0x1f
    cmp r1, #0x1a               /* ARM_MODE_HYP */
    bne frf_privilege_ready
    bic r0, r0, #0x1f
    orr r0, r0, #0xd3           /* SVC, IRQ and FIQ masked */
    msr spsr_cxsf, r0
    adr r1, frf_privilege_ready
    msr ELR_hyp, r1
    eret
frf_privilege_ready:
    /* Firmware loads SDRAM; do not initialize DRAM controller here.
       Turn off ARM D-cache before this test program touches shared
       VideoCore mailbox/scanout memory. MMU is firmware-disabled. */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #4            /* SCTLR.C: D-cache off */
    bic r0, r0, #0x2000       /* SCTLR.V: use programmable VBAR */
    mcr p15, 0, r0, c1, c0, 0
    dsb sy
    isb
#if defined(FRF_HAL_HARD_FLOAT) && FRF_HAL_HARD_FLOAT
    /* Permit AArch32 PL1/PL0 CP10/CP11 VFP and NEON execution.
       The firmware may leave CPACR/FPEXC disabled. This is per-core. */
    mrc p15, 0, r0, c1, c0, 2  /* CPACR */
    orr r0, r0, #0x00f00000  /* CP10 and CP11 full access */
    mcr p15, 0, r0, c1, c0, 2
    isb
    mov r0, #0x40000000       /* FPEXC.EN */
    vmsr fpexc, r0
    isb
#endif

    cps #0x13                   /* SVC */
    ldr sp, =frf_svc_stack_end
    cps #0x1b                   /* Undefined instruction */
    ldr sp, =frf_und_stack_end
    cps #0x17                   /* Abort */
    ldr sp, =frf_abt_stack_end
    cps #0x12                   /* IRQ */
    ldr sp, =frf_irq_stack_end
    cps #0x11                   /* FIQ */
    ldr sp, =frf_fiq_stack_end
    cps #0x13                   /* SVC */
    ldr sp, =frf_svc_stack_end

    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:
    cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b
2:
    dsb sy
    ldr r0, =frf_exception_vectors
    mcr p15, 0, r0, c12, c0, 0  /* VBAR: ARMv7-A exception table */
    dsb sy
    isb
    bl frf_hal_main
3:
    wfe
    b 3b

frf_secondary_park:
    /* Park with core id in r4. Primary clears BSS before releasing us.
       Support both firmware-spun mailbox3 entry and cores already in _start. */
    mov r4, r0
    ldr r5, =frf_secondary_release_mask
    mov r7, #1
    lsl r7, r7, r4
4:
    ldr r6, [r5]
    tst r6, r7
    bne 8f
    wfe
    b 4b
8:
    dmb sy
    /* Secondary cores may also be handed to us by firmware in HYP mode. */
    mrs r0, cpsr
    and r1, r0, #0x1f
    cmp r1, #0x1a
    bne 5f
    bic r0, r0, #0x1f
    orr r0, r0, #0xd3
    msr spsr_cxsf, r0
    adr r1, 5f
    msr ELR_hyp, r1
    eret
5:
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #4
    bic r0, r0, #0x2000
    mcr p15, 0, r0, c1, c0, 0
    dsb sy
    isb
#if defined(FRF_HAL_HARD_FLOAT) && FRF_HAL_HARD_FLOAT
    /* Permit AArch32 PL1/PL0 CP10/CP11 VFP and NEON execution.
       The firmware may leave CPACR/FPEXC disabled. This is per-core. */
    mrc p15, 0, r0, c1, c0, 2  /* CPACR */
    orr r0, r0, #0x00f00000  /* CP10 and CP11 full access */
    mcr p15, 0, r0, c1, c0, 2
    isb
    mov r0, #0x40000000       /* FPEXC.EN */
    vmsr fpexc, r0
    isb
#endif

    cps #0x13
    ldr r0, =frf_secondary_stacks
    add r0, r0, r4, lsl #12
    mov sp, r0
    ldr r0, =frf_exception_vectors
    mcr p15, 0, r0, c12, c0, 0
    isb
    mov r0, r4
    bl frf_hal_secondary_entry
6:
    wfe
    b 6b
.size _start, . - _start

/* A build without smp.c never releases secondaries. The fallback is
   an inert loop; linking smp.c provides the strong worker entry. */
.weak frf_hal_secondary_entry
.type frf_hal_secondary_entry, %function
frf_hal_secondary_entry:
7:
    wfe
    b 7b


.section .text, "ax", %progbits
.balign 32
.global frf_exception_vectors
frf_exception_vectors:
    b frf_vector_reset
    b frf_vector_undefined
    b frf_vector_svc
    b frf_vector_prefetch
    b frf_vector_data_abort
    b frf_vector_reserved
    b frf_vector_irq
    b frf_vector_fiq

frf_vector_reset:
    b _start
frf_vector_undefined:
    mov r0, #1
    b frf_fault
frf_vector_svc:
    mov r0, #2
    b frf_fault
frf_vector_prefetch:
    mov r0, #3
    b frf_fault
frf_vector_data_abort:
    mov r0, #4
    b frf_fault
frf_vector_reserved:
    mov r0, #5
    b frf_fault
frf_vector_irq:
    /* IRQ LR points four bytes past the interrupted instruction.
       Save volatile AAPCS registers and return with CPSR restored. */
    stmdb sp!, {r0-r3, r12, lr}
    bl frf_hal_irq_handler
    ldmia sp!, {r0-r3, r12, lr}
    subs pc, lr, #4
frf_vector_fiq:
    mov r0, #7
    b frf_fault
frf_fault:
    mov r1, lr
    mrs r2, spsr
    b frf_hal_exception_panic

/* Separate exception-mode stacks avoid corrupting SVC while diagnosing a fault. */
.section .bss.stacks, "aw", %nobits
.balign 16
frf_und_stack: .space 1024
frf_und_stack_end:
frf_abt_stack: .space 1024
frf_abt_stack_end:
frf_irq_stack: .space 2048
frf_irq_stack_end:
frf_fiq_stack: .space 1024
frf_fiq_stack_end:
frf_svc_stack: .space 16384
frf_svc_stack_end:

/* Release flag must be present in the loaded image, initialized to
   zero BEFORE cores 1..3 can look at it, not in uncleared BSS. */
.section .data.smp, "aw", %progbits
.balign 64
.global frf_secondary_release_mask
frf_secondary_release_mask: .word 0

/* Separate 4 KiB SVC stacks for secondary cores. */
.section .bss.smp, "aw", %nobits
.balign 16
frf_secondary_stacks: .space 12288
