/*
 * FRF HAL host unit test of BCM2837 DMA bus translation and bounds.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../dma.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    assert(frf_hal_dma_bus_address((void *)(uintptr_t)0u) == 0u);
    assert(frf_hal_dma_bus_address((void *)(uintptr_t)0x1000u) == 0xC0001000u);
    assert(frf_hal_dma_bus_address((void *)(uintptr_t)0x3EFFFFFFu) == 0xFEFFFFFFu);
    assert(frf_hal_dma_bus_address((void *)(uintptr_t)0x3F000000u) == 0u);
    assert(frf_hal_dma_bus_address((void *)(uintptr_t)0x40000000u) == 0u);
    assert(frf_hal_dcache_line_bytes() == 64u);
    assert(frf_hal_dcache_enabled() == 0);
    assert(frf_hal_dma_clean((void *)(uintptr_t)0x1000u, 64u) == 1);
    assert(frf_hal_dma_invalidate((void *)(uintptr_t)0x1003u, 64u) == 1);
    assert(frf_hal_dma_clean((void *)(uintptr_t)0x3EFFFFF0u, 0x20u) == 0);
    assert(frf_hal_dma_invalidate((void *)(uintptr_t)0x3F000000u, 64u) == 0);
    puts("FRF HAL DMA mapping host tests: PASS");
    return 0;
}
