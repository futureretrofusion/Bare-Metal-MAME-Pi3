/*
 * FRF HAL Pi3 stage-0 serial boot self-check.
 * Copyright (c) 2026 Future Retro Fusion.
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <stdint.h>
extern unsigned char __image_end__[];
static volatile uint32_t bss_probe;

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL Pi3 / independent ARM32 boot\n");
    frf_hal_uart_puts("Build: Stage 0, no Circle, no MAME linked\n");
    frf_hal_uart_puts("Image RAM end: ");
    frf_hal_uart_hex32((uint32_t)(uintptr_t)__image_end__);
    frf_hal_uart_puts("\nBSS: ");
    if (bss_probe != 0u) {
        frf_hal_uart_puts("FAIL\n");
        for (;;) __asm__ volatile("wfe");
    }
    bss_probe = 0x465246u;
    frf_hal_uart_puts("PASS\n");
    uint64_t before = frf_hal_time_us();
    frf_hal_delay_us(10000u);
    uint64_t after = frf_hal_time_us();
    frf_hal_uart_puts("System timer 10ms monotonic: ");
    frf_hal_uart_puts(after - before >= 10000ull ? "PASS\n" : "FAIL\n");
    for (;;) {
        frf_hal_delay_us(1000000u);
        frf_hal_uart_puts("FRF HAL heartbeat 1s\n");
    }
}
