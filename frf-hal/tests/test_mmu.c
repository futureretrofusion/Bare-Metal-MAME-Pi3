/*
 * Host-unit checks for FRF's independent identity map descriptors.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../mmu.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t table[FRF_L1_ENTRIES];
int main(void)
{
    frf_hal_mmu_build_table(table);
    assert((table[0] & FRF_MMU_SECTION) == FRF_MMU_SECTION);
    assert((table[0] & FRF_MMU_CACHE_WBWA) == FRF_MMU_CACHE_WBWA);
    assert((table[0] & FRF_MMU_XN) == 0u);
    assert((table[0x100] & 0xFFF00000u) == 0x10000000u);
    assert((table[0x3EF] & FRF_MMU_CACHE_WBWA) == FRF_MMU_CACHE_WBWA);
    assert((table[0x3F0] & ((1u << 12) | (1u << 3))) == 0u);
    assert((table[0x3F0] & (FRF_MMU_DEVICE|FRF_MMU_XN)) ==
           (FRF_MMU_DEVICE|FRF_MMU_XN));
    assert((table[0x400] & (FRF_MMU_DEVICE|FRF_MMU_XN)) ==
           (FRF_MMU_DEVICE|FRF_MMU_XN));
    assert(table[0x401] == 0u);
    assert(table[0x800] == 0u);
    assert(table[0xFFF] == 0u);
    puts("FRF HAL MMU descriptor host tests: PASS");
    return 0;
}
