/*
 * Independent BCM2837 VideoCore property mailbox transport.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Protocol: Raspberry Pi firmware Mailbox Property Interface documentation.
 * This is a fresh implementation, not a copy/adaptation of Circle code.
 */
#include "mailbox.h"
#include "hal.h"
#include <stdint.h>

#define MB_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0xB880u)
#define MB_READ (MB_BASE + 0x00u)
#define MB_READ_STATUS (MB_BASE + 0x18u)
#define MB_WRITE (MB_BASE + 0x20u)
#define MB_WRITE_STATUS (MB_BASE + 0x38u)
#define MB_FULL (1u << 31)
#define MB_EMPTY (1u << 30)
#define MB_CHANNEL_PROPERTY 8u
#define MB_TIMEOUT_US 2000000ull

static uint32_t reg32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}
static void store32(uint32_t address, uint32_t data)
{
    *(volatile uint32_t *)(uintptr_t)address = data;
}
static void ordered(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

int frf_hal_property_call(volatile uint32_t *message, unsigned words)
{
    const uintptr_t address = (uintptr_t)message;
    if (!message || (address & 15u) || address >= 0x40000000u ||
        words < 3u || words > 256u || message[0] != words * 4u ||
        message[1] != 0u) return 0;

    /* Property channel 8 uses ARM physical addresses. The buffer is
       shareable only while the ARM MMU/data caches remain disabled.
       Mailbox 1 is ARM->VC, mailbox 0 is VC->ARM. */
    const uint32_t token = (uint32_t)address | MB_CHANNEL_PROPERTY;
    const uint64_t start = frf_hal_time_us();
    ordered();
    while ((reg32(MB_WRITE_STATUS) & MB_FULL) != 0u) {
        if (frf_hal_time_us() - start >= MB_TIMEOUT_US) return 0;
    }
    store32(MB_WRITE, token);
    for (;;) {
        if (frf_hal_time_us() - start >= MB_TIMEOUT_US) return 0;
        if ((reg32(MB_READ_STATUS) & MB_EMPTY) != 0u) continue;
        const uint32_t reply = reg32(MB_READ);
        if (reply != token) continue;
        ordered();
        return message[1] == 0x80000000u;
    }
}
