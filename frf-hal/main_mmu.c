/*
 * Independent FRF HAL Pi3 ARM32 MMU+I/D cache smoke test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No firmware mailbox, GPU framebuffer or DMA use after caches start.
 */
#include "hal.h"
#include "mmu.h"
#include <stdint.h>
static volatile uint32_t cache_probe[512] __attribute__((aligned(64)));

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / MMU + cache isolation diagnostic\n");
    const uint64_t t0 = frf_hal_time_us();
    frf_hal_mmu_enable();
    frf_hal_uart_puts("MMU enabled, RAM WBWA, peripheral/device XN\n");
    uint32_t checksum = 0u;
    for (uint32_t i = 0; i != 512u; ++i) cache_probe[i] = i ^ 0xA5A50000u;
    for (uint32_t i = 0; i != 512u; ++i)
        checksum ^= (cache_probe[i] ^ i ^ 0xA5A50000u);
    frf_hal_uart_puts(checksum == 0u ? "Cache RAM probe: PASS\n" :
                                      "Cache RAM probe: FAIL\n");
    frf_hal_uart_puts(frf_hal_time_us() >= t0 ?
                      "Timer under MMU: PASS\n" : "Timer under MMU: FAIL\n");
    for (;;) {
        frf_hal_delay_us(1000000u);
        frf_hal_uart_puts("MMU HEARTBEAT\n");
    }
}
