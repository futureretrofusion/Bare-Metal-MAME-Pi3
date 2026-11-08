/*
 * FRF HAL Pi3 Stage 2: independent ARM IRQ and 100 Hz timer smoke test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "irq.h"
#include <stdint.h>

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / Stage 2 ARM IRQ diagnostic\n");
    frf_hal_irq_timer_start(10000u);
    uint32_t previous = frf_hal_irq_ticks;
    for (;;) {
        frf_hal_delay_us(1000000u);
        uint32_t count = frf_hal_irq_ticks;
        uint32_t spurious = frf_hal_irq_spurious;
        frf_hal_uart_puts("Timer IRQ ticks: ");
        frf_hal_uart_hex32(count);
        frf_hal_uart_puts(" delta: ");
        frf_hal_uart_hex32(count - previous);
        frf_hal_uart_puts(" spurious: ");
        frf_hal_uart_hex32(spurious);
        frf_hal_uart_puts("\n");
        if (count - previous >= 80u && count - previous <= 120u &&
            spurious == 0u)
            frf_hal_uart_puts("IRQ 1-second count: PASS\n");
        else
            frf_hal_uart_puts("IRQ 1-second count: FAIL\n");
        previous = count;
    }
}
