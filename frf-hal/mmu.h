/*
 * Future Retro Fusion / BCM2837 independent short-descriptor MMU.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Pi 3 only; MMU init is single-core and is not yet a DMA-safe MAME map.
 */
#ifndef FRF_HAL_MMU_H
#define FRF_HAL_MMU_H
#include <stdint.h>
enum { FRF_L1_ENTRIES = 4096, FRF_RAM_SECTIONS = 0x3F0 };
/* Descriptors for host verification as well as ARM32 firmware. */
#define FRF_MMU_SECTION 0x00000002u
#define FRF_MMU_RW_PL1 (1u << 10)
#define FRF_MMU_SHAREABLE (1u << 16)
#define FRF_MMU_XN (1u << 4)
#define FRF_MMU_CACHE_WBWA ((1u << 12) | (1u << 3) | (1u << 2))
#define FRF_MMU_DEVICE 0x4u
void frf_hal_mmu_build_table(uint32_t table[FRF_L1_ENTRIES]);
const uint32_t *frf_hal_mmu_table(void);
void frf_hal_mmu_enable(void);
/* This will enable D-cache. DO NOT call with active VideoCore/DWC2 DMA. */
void frf_hal_mmu_activate(uint32_t table_phys);
#endif
