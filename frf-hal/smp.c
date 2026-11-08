/*
 * FRF HAL: BCM2836/37 ARM-local mailbox3 wakeup + CPU0/worker handshake.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Firmware may park cores 1..3 at a mailbox3 spin stub, or deliver
 * the boot image directly to those cores. boot.S handles both cases.
 * Shared RAM is UN-CACHED in this isolated test (MMU disabled on all PEs).
 * Do NOT use this polling protocol unmodified with non-coherent caches.
 */
#include "hal.h"
#include "smp.h"
#include <stdint.h>
#include <stddef.h>

#define FRF_LOCAL_BASE 0x40000000u
#define FRF_MAILBOX3_SET(core) (FRF_LOCAL_BASE + 0x8Cu + (core)*16u)
#define FRF_MAILBOX3_CLEAR(core) (FRF_LOCAL_BASE + 0xCCu + (core)*16u)

extern volatile uint32_t frf_secondary_release_mask;
extern void _start(void);
static volatile uint32_t core_ready[4] __attribute__((aligned(64)));
static frf_smp_job jobs[4] __attribute__((aligned(64)));

static uint32_t read32(uint32_t addr)
{ return *(volatile uint32_t *)(uintptr_t)addr; }
static void write32(uint32_t addr, uint32_t value)
{ *(volatile uint32_t *)(uintptr_t)addr = value; }
static void send_event(void)
{ __asm__ volatile("dsb sy\n\tsev" ::: "memory"); }

void frf_hal_smp_start_all(void)
{
    /* This API is only safe once: startup cleared BSS; CPU0 owns setup. */
    frf_secondary_release_mask = 0xEu;
    frf_smp_barrier();
    for (uint32_t core = 1u; core <= 3u; ++core) {
        /* For standard firmware spin table. Cores already in _start
         * consume the release mask and clear mailbox3 themselves. */
        write32(FRF_MAILBOX3_SET(core), (uint32_t)(uintptr_t)&_start);
    }
    send_event();
}
int frf_hal_smp_ready(unsigned core)
{
    if (core == 0u || core > 3u) return 0;
    frf_smp_barrier();
    return core_ready[core] == 0x46524631u + core;
}
int frf_hal_smp_ping(unsigned core, uint32_t value, uint32_t timeout_us)
{
    if (!frf_hal_smp_ready(core) || timeout_us == 0u) return 0;
    frf_smp_job *j = &jobs[core];
    const uint32_t n = j->sequence + 1u;
    j->payload = value;
    frf_smp_barrier();
    j->sequence = n;
    send_event();
    const uint64_t start = frf_hal_time_us();
    while (j->acknowledged != n) {
        if (frf_hal_time_us() - start >= (uint64_t)timeout_us) return 0;
        __asm__ volatile("yield" ::: "memory");
    }
    frf_smp_barrier();
    return j->response == frf_smp_response(core, value);
}
void frf_hal_secondary_entry(unsigned core)
{
    if (core == 0u || core > 3u)
        for (;;) __asm__ volatile("wfe");
    /* mailbox3 set by CPU0 may be pending if firmware passed all cores
       directly into _start. Read/clear only core's own mailbox3. */
    const uint32_t mailbox = FRF_MAILBOX3_CLEAR(core);
    const uint32_t pending = read32(mailbox);
    if (pending) write32(mailbox, pending);
    frf_smp_barrier();
    core_ready[core] = 0x46524631u + core;
    send_event();
    for (;;) {
        frf_smp_job *j = &jobs[core];
        if (j->sequence != j->acknowledged) {
            frf_smp_process_once(j, core);
            send_event();
        } else {
            __asm__ volatile("wfe");
        }
    }
}
