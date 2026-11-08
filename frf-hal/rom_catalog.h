/*
 * FRF HAL ROM archive directory-to-MAME driver index adapter.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Does not contain ROMs, MAME CPU code or Circle code.
 * Caller owns driver table; names and descriptions come from MAME core.
 */
#ifndef FRF_HAL_ROM_CATALOG_H
#define FRF_HAL_ROM_CATALOG_H
#include <stdint.h>
#include "fat32.h"
enum { FRF_ROM_MAX_DRIVERS=8192u };
typedef struct {
    const char *name;        /* MAME short name ("galaga") */
    const char *description; /* Menu title */
} frf_rom_driver;
typedef struct {
    /* Driver IDs in menu display order. Caller provides this buffer. */
    uint16_t selected[FRF_ROM_MAX_DRIVERS];
    /* Temporary driver lookup indices sorted by ROM short name. */
    uint16_t by_shortname[FRF_ROM_MAX_DRIVERS];
    uint8_t seen[FRF_ROM_MAX_DRIVERS/8u];
    uint16_t selected_count;
    uint16_t driver_count; /* max 8192 cannot be represented as uint16? 8192 fits */
    uint32_t inspected;
    uint32_t zip_count;
} frf_rom_catalog;
int frf_rom_catalog_scan(frf_fat32 *fs, const frf_rom_driver *drivers,
                         uint32_t count, frf_rom_catalog *catalog);
#endif
