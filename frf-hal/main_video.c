/*
 * FRF HAL Pi3: existing FRF integer scaler + scanline renderer,
 * independent mailbox/HDMI double-buffer smoke test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No MAME core, Circle, GPL screen driver or SD/USB linked here.
 */
#include "hal.h"
#include "framebuffer.h"
#include "../baremetal/bm_platform.h"
#include <stdint.h>
#include <stddef.h>
static frf_hal_framebuffer fb;
static uint16_t source[320u * 240u] __attribute__((aligned(64)));
static int present(unsigned page) { return frf_hal_fb_flip(&fb, page); }

static void make_frame(unsigned frame)
{
    for (unsigned y = 0u; y < 240u; ++y)
        for (unsigned x = 0u; x < 320u; ++x) {
            uint16_t color = (uint16_t)((((x + frame) >> 3u) & 31u) << 11u);
            color |= (uint16_t)(((y >> 2u) & 63u) << 5u);
            color |= (uint16_t)((x >> 3u) & 31u);
            if (x >= (frame % 288u) && x < (frame % 288u) + 32u)
                color = 0xFFFFu;
            source[y * 320u + x] = color;
        }
}
void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / original FRF video-renderer diagnostic\n");
    if (!frf_hal_fb_open(&fb, 640u, 480u)) {
        frf_hal_uart_puts("Framebuffer allocation failed; HALTED\n");
        for (;;) __asm__ volatile("wfe");
    }
    bm_video_attach((void *)(uintptr_t)fb.physical_address,
                    fb.width, fb.height, fb.pitch_bytes);
    bm_video_configure_double(fb.virtual_height, fb.buffer_bytes, present);
    if (!bm_video_is_double()) {
        frf_hal_uart_puts("Double buffering not available; HALTED\n");
        for (;;) __asm__ volatile("wfe");
    }
    bm_video_set_orientation(0u);
    bm_video_set_scanlines(96u);
    bm_video_begin_game();
    frf_hal_uart_puts("Renderer attached: RGB565, 2x integer, scanlines\n");
    unsigned frame = 0u;
    for (;;) {
        const uint64_t start = frf_hal_time_us();
        make_frame(frame);
        bm_submit_frame(source, 320u, 240u, 320u * 2u, 16u, 0, 0u);
        ++frame;
        if ((frame % 60u) == 0u) {
            frf_hal_uart_puts("FRF VIDEO frames=");
            frf_hal_uart_hex32(frame);
            frf_hal_uart_puts(" flips=");
            frf_hal_uart_hex32(frf_bm_flip_success);
            frf_hal_uart_puts(" failures=");
            frf_hal_uart_hex32(frf_bm_flip_failure);
            frf_hal_uart_puts("\n");
        }
        /* Target 50Hz. Firmware VBlank tag may extend duration; a
           Pi3 display capture is needed to determine actual cadence. */
        const uint64_t elapsed = frf_hal_time_us() - start;
        if (elapsed < 20000ull) frf_hal_delay_us((uint32_t)(20000ull-elapsed));
    }
}
