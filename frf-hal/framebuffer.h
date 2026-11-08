/*
 * FRF HAL 16-bit VideoCore framebuffer + two-page diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_FRAMEBUFFER_H
#define FRF_HAL_FRAMEBUFFER_H
#include <stdint.h>
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;
    uint32_t virtual_height;
    uint32_t physical_address;
    uint32_t buffer_bytes;
} frf_hal_framebuffer;
int frf_hal_fb_open(frf_hal_framebuffer *fb, uint32_t width, uint32_t height);
int frf_hal_fb_flip(const frf_hal_framebuffer *fb, unsigned page);
void frf_hal_fb_draw_test_page(const frf_hal_framebuffer *fb, unsigned page);
#endif
