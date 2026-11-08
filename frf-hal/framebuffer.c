/*
 * Independent BCM2837 property-framebuffer diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No Circle framebuffer or screen implementations are incorporated.
 */
#include "framebuffer.h"
#include "mailbox.h"
#include <stdint.h>

static volatile uint32_t fb_request[30] __attribute__((aligned(16)));
static volatile uint32_t flip_request[12] __attribute__((aligned(16)));

int frf_hal_fb_open(frf_hal_framebuffer *fb, uint32_t width, uint32_t height)
{
    if (!fb || width == 0u || height == 0u ||
        width > 1280u || height > 720u) return 0;

    /* Set physical, set virtual, depth, pixel order, allocate, get pitch.
       Property protocol requires one request with all FB setup tags. */
    volatile uint32_t *p = fb_request;
    for (unsigned i = 0; i < 30u; ++i) p[i] = 0u;
    p[0] = 30u * 4u;
    p[2] = 0x00048003u; p[3] = 8u; p[4] = 8u;
    p[5] = width; p[6] = height;
    p[7] = 0x00048004u; p[8] = 8u; p[9] = 8u;
    p[10] = width; p[11] = height * 2u;
    p[12] = 0x00048005u; p[13] = 4u; p[14] = 4u; p[15] = 16u;
    p[16] = 0x00048006u; p[17] = 4u; p[18] = 4u; p[19] = 1u;
    p[20] = 0x00040001u; p[21] = 8u; p[22] = 4u; p[23] = 4096u;
    p[25] = 0x00040008u; p[26] = 4u; p[27] = 0u;

    if (!frf_hal_property_call(p, 30u)) return 0;
    if ((p[4] & 0x80000000u) == 0u ||
        (p[9] & 0x80000000u) == 0u ||
        (p[14] & 0x80000000u) == 0u ||
        (p[18] & 0x80000000u) == 0u ||
        (p[22] & 0x80000000u) == 0u ||
        (p[27] & 0x80000000u) == 0u) return 0;
    const uint32_t pitch = p[28];
    const uint32_t vh = p[11];
    const uint32_t bus = p[23];
    const uint32_t count = p[24];
    if (p[5] != width || p[6] != height ||
        p[10] != width || vh < height * 2u ||
        p[15] != 16u || p[19] != 1u ||
        !bus || pitch < width * 2u ||
        (pitch & 1u) != 0u || vh == 0u || pitch > UINT32_MAX / vh ||
        count < pitch * vh) return 0;

    fb->width = width;
    fb->height = height;
    fb->pitch_bytes = pitch;
    fb->virtual_height = vh;
    fb->physical_address = bus & 0x3FFFFFFFu; /* VC alias to ARM physical */
    fb->buffer_bytes = count;
    return 1;
}
int frf_hal_fb_flip(const frf_hal_framebuffer *fb, unsigned page)
{
    if (!fb || page > 1u || fb->virtual_height < 2u * fb->height) return 0;
    volatile uint32_t *p = flip_request;
    for (unsigned i = 0u; i < 12u; ++i) p[i] = 0u;
    p[0] = 12u * 4u;
    p[2] = 0x00048009u; p[3] = 8u; p[4] = 8u;
    p[5] = 0u; p[6] = page * fb->height;
    p[7] = 0x0004800Eu; p[8] = 4u; p[9] = 4u;
    /* Ordered offset+VBlank property tags; real hardware must establish
       whether this order fences the old scanout page reliably. */
    if (!frf_hal_property_call(p, 12u)) return 0;
    return ((p[4] & 0x80000000u) != 0u &&
            (p[9] & 0x80000000u) != 0u &&
            p[5] == 0u && p[6] == page * fb->height);
}
void frf_hal_fb_draw_test_page(const frf_hal_framebuffer *fb, unsigned page)
{
    if (!fb || page > 1u) return;
    const uint32_t off = page * fb->height * fb->pitch_bytes;
    for (uint32_t y = 0u; y < fb->height; ++y) {
        volatile uint16_t *row = (volatile uint16_t *)(uintptr_t)
            (fb->physical_address + off + y * fb->pitch_bytes);
        for (uint32_t x = 0u; x < fb->width; ++x) {
            uint16_t c = page ? 0x001Fu : 0xF800u; /* RGB565 blue/red */
            if ((x / 32u) % 2u == 0u && (y / 32u) % 2u == 0u) c = 0xFFFFu;
            if (x == fb->width / 2u || y == fb->height / 2u) c = 0x07E0u;
            row[x] = c;
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
