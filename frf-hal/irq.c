/*
 * Independent Raspberry Pi 3 system timer compare IRQ driver.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * System timer compare 1 uses the ARM interrupt-controller IRQ1 bit 1.
 * This isolated diagnostic controls all ARM IRQ enable banks, so it
 * must not be linked into a shared interrupt environment unchanged.
 */
#include "hal.h"
#include "irq.h"
#include <stdint.h>

#define IRQ_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0xB200u)
#define IRQ_PENDING1 (IRQ_BASE + 0x04u)
#define IRQ_ENABLE1 (IRQ_BASE + 0x10u)
#define IRQ_DISABLE1 (IRQ_BASE + 0x1Cu)
#define IRQ_DISABLE2 (IRQ_BASE + 0x20u)
#define IRQ_DISABLE_BASIC (IRQ_BASE + 0x24u)
#define SYS_TIMER_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0x3000u)
#define SYS_TIMER_CS (SYS_TIMER_BASE + 0x00u)
#define SYS_TIMER_LO (SYS_TIMER_BASE + 0x04u)
#define SYS_TIMER_C1 (SYS_TIMER_BASE + 0x10u)
#define TIMER_C1_BIT (1u << 1)
volatile uint32_t frf_hal_irq_ticks;
volatile uint32_t frf_hal_irq_spurious;
static uint32_t timer_period_us = 10000u;

static uint32_t reg32(uint32_t addr)
{
    return *(volatile uint32_t *)(uintptr_t)addr;
}
static void store32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)addr = value;
}
static void irq_mask_all(void)
{
    store32(IRQ_DISABLE1, 0xFFFFFFFFu);
    store32(IRQ_DISABLE2, 0xFFFFFFFFu);
    store32(IRQ_DISABLE_BASIC, 0xFFFFFFFFu);
    __asm__ volatile("dsb sy" ::: "memory");
}

void frf_hal_irq_timer_start(uint32_t period_us)
{
    __asm__ volatile("cpsid i" ::: "memory");
    irq_mask_all();
    frf_hal_irq_ticks = 0u;
    frf_hal_irq_spurious = 0u;
    /* This test is intended to run at 100 Hz; avoid 32-bit timeout edge. */
    if (period_us < 1000u || period_us > 1000000u) period_us = 10000u;
    timer_period_us = period_us;
    store32(SYS_TIMER_CS, TIMER_C1_BIT); /* write-one-to-clear */
    store32(SYS_TIMER_C1, reg32(SYS_TIMER_LO) + timer_period_us);
    store32(IRQ_ENABLE1, TIMER_C1_BIT);
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("cpsie i" ::: "memory");
}
void frf_hal_irq_timer_stop(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
    store32(IRQ_DISABLE1, TIMER_C1_BIT);
    store32(SYS_TIMER_CS, TIMER_C1_BIT);
    __asm__ volatile("dsb sy" ::: "memory");
}
void frf_hal_irq_handler(void)
{
    /* Other interrupt sources are masked in the independent test. */
    const uint32_t pending = reg32(IRQ_PENDING1);
    if ((pending & TIMER_C1_BIT) != 0u &&
        (reg32(SYS_TIMER_CS) & TIMER_C1_BIT) != 0u) {
        store32(SYS_TIMER_CS, TIMER_C1_BIT);
        store32(SYS_TIMER_C1, reg32(SYS_TIMER_LO) + timer_period_us);
        ++frf_hal_irq_ticks;
    } else {
        ++frf_hal_irq_spurious;
        /* Avoid an interrupt storm if unexpected firmware IRQ is enabled. */
        irq_mask_all();
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
