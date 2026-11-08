/*
 * FRF HAL BCM2837 DMA address conversion and ARM32 cache maintenance.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No Linux/Circle dependencies. Cache-alignment enforcement protects
 * neighboring dirty bytes. With D-cache ON, callers MUST own buffers.
 */
#include "dma.h"
#include <stdint.h>
#include <stddef.h>
static void barrier(void)
{
#if defined(__arm__)
    __asm__ volatile("dsb sy" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
uintptr_t frf_hal_dma_bus_address(const void *ptr)
{
    uintptr_t p = (uintptr_t)ptr;
    if (p == 0u || p >= 0x3F000000u) return 0u;
    return p | 0xC0000000u;
}
uint32_t frf_hal_dcache_line_bytes(void)
{
#if defined(__arm__)
    uint32_t ctr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
    /* ARMv7 CTR.DminLine: log2(words of 4 bytes). */
    return 4u << ((ctr >> 16) & 15u);
#else
    return 64u;
#endif
}
int frf_hal_dcache_enabled(void)
{
#if defined(__arm__)
    uint32_t ctl;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(ctl));
    return (ctl & 4u) != 0u;
#else
    return 0;
#endif
}
static int valid_range(const void *ptr, uint32_t n)
{
    uintptr_t p = (uintptr_t)ptr;
    return p != 0u && p < 0x3F000000u &&
           n != 0u && (uintptr_t)n <= 0x3F000000u - p;
}
int frf_hal_dma_clean(const void *ptr, uint32_t size)
{
    if (!valid_range(ptr, size)) return 0;
    if (!frf_hal_dcache_enabled()) { barrier(); return 1; }
    const uint32_t line = frf_hal_dcache_line_bytes();
    if (!line || (line & (line - 1u))) return 0;
    const uintptr_t start = (uintptr_t)ptr & ~(uintptr_t)(line - 1u);
    const uintptr_t end = (uintptr_t)ptr + size;
#if defined(__arm__)
    for (uintptr_t p = start; p < end; p += line)
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(p) : "memory");
#else
    (void)start; (void)end;
#endif
    barrier();
    return 1;
}
int frf_hal_dma_invalidate(const void *ptr, uint32_t size)
{
    if (!valid_range(ptr, size)) return 0;
    if (!frf_hal_dcache_enabled()) { barrier(); return 1; }
    const uint32_t line = frf_hal_dcache_line_bytes();
    if (!line || (line & (line - 1u)) ||
        ((uintptr_t)ptr & (line - 1u)) || (size & (line - 1u)))
        return 0;
    const uintptr_t end = (uintptr_t)ptr + size;
    barrier(); /* DMA must already have completed before this call. */
#if defined(__arm__)
    for (uintptr_t p = (uintptr_t)ptr; p < end; p += line)
        __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(p) : "memory");
#else
    (void)end;
#endif
    barrier();
    return 1;
}
