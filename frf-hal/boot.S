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
    mrc p15, 0, r0, c0, c0, 5   /* MPIDR: low bits = Pi 3 core index */
    and r0, r0, #3
    cmp r0, #0
    bne frf_secondary_park

    /* The firmware has already loaded the image and configured SDRAM.
       Establish our own stacks and exception vector base. */
    cps #0x13                     /* SVC */
    ldr sp, =frf_svc_stack_end
    cps #0x1b                     /* Undefined instruction */
    ldr sp, =frf_und_stack_end
    cps #0x17                     /* Abort */
    ldr sp, =frf_abt_stack_end
    cps #0x12                     /* IRQ */
    ldr sp, =frf_irq_stack_end
    cps #0x11                     /* FIQ */
    ldr sp, =frf_fiq_stack_end
    cps #0x13                     /* SVC */
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
    /* No secondary-core release is attempted at this milestone. */
4:
    wfe
    b 4b
.size _start, . - _start

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
    mov r0, #6
    b frf_fault
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
