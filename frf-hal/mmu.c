/*
 * Future Retro Fusion / BCM2837 L1 section mapping.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * ARMv7-A short-descriptor, 16 KiB aligned TTBR0.
 *
 * 0x00000000..0x3EFFFFFF  identity normal shareable WBWA RAM (diagnostic)
 * 0x3F000000..0x3FFFFFFF  BCM2837 peripherals, Device, XN
 * 0x40000000..0x400FFFFF  BCM2836/7 local registers, Device, XN
 * Remaining VA range faults intentionally.
 *
 * The diagnostic does not call the firmware mailbox or run DWC2 once
 * D-cache is enabled; framebuffer and USB memory coherency remains open.
 */
#include "mmu.h"
#include <stdint.h>

static uint32_t frf_l1[FRF_L1_ENTRIES] __attribute__((aligned(16384)));

void frf_hal_mmu_build_table(uint32_t table[FRF_L1_ENTRIES])
{
    if (!table) return;
    for (uint32_t i = 0; i != FRF_L1_ENTRIES; ++i) table[i] = 0u;
    for (uint32_t i = 0; i < FRF_RAM_SECTIONS; ++i) {
        table[i] = (i << 20) | FRF_MMU_SECTION | FRF_MMU_RW_PL1 |
                   FRF_MMU_SHAREABLE | FRF_MMU_CACHE_WBWA;
    }
    for (uint32_t i = FRF_RAM_SECTIONS; i < 0x401u; ++i) {
        table[i] = (i << 20) | FRF_MMU_SECTION | FRF_MMU_RW_PL1 |
                   FRF_MMU_SHAREABLE | FRF_MMU_XN | FRF_MMU_DEVICE;
    }
}
const uint32_t *frf_hal_mmu_table(void) { return frf_l1; }
void frf_hal_mmu_enable(void)
{
    frf_hal_mmu_build_table(frf_l1);
    /* Table physically resident below 1 GiB; boot image identity mapped. */
    frf_hal_mmu_activate((uint32_t)(uintptr_t)frf_l1);
}
