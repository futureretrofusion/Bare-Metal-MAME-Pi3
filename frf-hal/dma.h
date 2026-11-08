/*
 * FRF HAL Pi3 DMA addressing/cache-maintenance interface.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * BCM2837 only. External DMA still requires ownership and barriers.
 */
#ifndef FRF_HAL_DMA_H
#define FRF_HAL_DMA_H
#include <stdint.h>
#include <stddef.h>
/* Returns 0 for invalid/null address, otherwise C0000000 GPU bus alias. */
uintptr_t frf_hal_dma_bus_address(const void *ptr);
uint32_t frf_hal_dcache_line_bytes(void);
int frf_hal_dcache_enabled(void);
/* Return 1 on success, 0 on unsupported or unsafe alignment.
 * Invalidate requires whole cacheline buffers while D-cache is enabled. */
int frf_hal_dma_clean(const void *ptr, uint32_t size);
int frf_hal_dma_invalidate(const void *ptr, uint32_t size);
#endif
