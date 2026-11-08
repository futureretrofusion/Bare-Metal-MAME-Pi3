/*
 * FRF HAL independent framebuffer visual smoke test (Stage 1).
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "framebuffer.h"
#include <stdint.h>
static frf_hal_framebuffer framebuffer;

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / Stage 1 mailbox framebuffer diagnostic\n");
    uint64_t t0 = frf_hal_time_us();
    frf_hal_delay_us(10000u);
    if (frf_hal_time_us() - t0 < 10000ull)
        frf_hal_uart_puts("System timer FAIL\n");
    else
        frf_hal_uart_puts("System timer PASS\n");
    if (!frf_hal_fb_open(&framebuffer, 640u, 480u)) {
        frf_hal_uart_puts("Framebuffer allocation FAILED; serial heartbeat only\n");
        for (;;) {
            frf_hal_delay_us(1000000u);
            frf_hal_uart_puts("FB allocation retry requires reboot\n");
        }
    }
    frf_hal_uart_puts("FB physical=");
    frf_hal_uart_hex32(framebuffer.physical_address);
    frf_hal_uart_puts(" pitch=");
    frf_hal_uart_hex32(framebuffer.pitch_bytes);
    frf_hal_uart_puts(" bytes=");
    frf_hal_uart_hex32(framebuffer.buffer_bytes);
    frf_hal_uart_puts("\n");
    frf_hal_fb_draw_test_page(&framebuffer, 0u);
    frf_hal_fb_draw_test_page(&framebuffer, 1u);
    unsigned page = 0u;
    for (;;) {
        if (!frf_hal_fb_flip(&framebuffer, page))
            frf_hal_uart_puts("FB flip FAILED\n");
        else
            frf_hal_uart_puts(page ? "FB page 1 displayed\n" :
                                    "FB page 0 displayed\n");
        page ^= 1u;
        frf_hal_delay_us(1000000u);
    }
}
