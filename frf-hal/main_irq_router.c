/*
 * FRF HAL BCM2837 shared IRQ router with system timer channel1 test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "irq_router.h"
#include <stdint.h>
#define TIMER (FRF_BCM2837_PERIPHERAL_BASE + 0x3000u)
#define TIMER_CS (TIMER + 0x00u)
#define TIMER_CLO (TIMER + 0x04u)
#define TIMER_C1 (TIMER + 0x10u)
static volatile uint32_t tick_count;
static void timer_irq(unsigned irq, void *ctx)
{
    (void)irq; (void)ctx;
    *(volatile uint32_t *)(uintptr_t)TIMER_CS = 2u;
    *(volatile uint32_t *)(uintptr_t)TIMER_C1 =
          *(volatile uint32_t *)(uintptr_t)TIMER_CLO + 10000u;
    ++tick_count;
}
void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / shared IRQ router diagnostic\n");
    frf_hal_irq_router_init();
    (void)frf_hal_irq_attach(1u, timer_irq, 0);
    *(volatile uint32_t *)(uintptr_t)TIMER_CS = 2u;
    *(volatile uint32_t *)(uintptr_t)TIMER_C1 =
        *(volatile uint32_t *)(uintptr_t)TIMER_CLO + 10000u;
    if (!frf_hal_irq_enable(1u)) {
        frf_hal_uart_puts("Timer IRQ attach failed\n");
        for (;;) __asm__ volatile("wfe");
    }
    __asm__ volatile("cpsie i" ::: "memory");
    uint32_t last = 0u;
    for (;;) {
        frf_hal_delay_us(1000000u);
        const uint32_t now = tick_count;
        const uint32_t delta = now - last;
        last = now;
        frf_hal_uart_puts("IRQ ticks/second: ");
        frf_hal_uart_hex32(delta);
        frf_hal_uart_puts(" dispatched: ");
        frf_hal_uart_hex32(frf_hal_irq_dispatch_count);
        frf_hal_uart_puts(" unhandled: ");
        frf_hal_uart_hex32(frf_hal_irq_unhandled_count);
        frf_hal_uart_puts("\n");
    }
}
