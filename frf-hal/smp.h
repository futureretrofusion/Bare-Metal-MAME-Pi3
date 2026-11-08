/*
 * FRF HAL independent Raspberry Pi 3 multicore diagnostic protocol.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_SMP_H
#define FRF_HAL_SMP_H
#include <stdint.h>
typedef struct {
    volatile uint32_t sequence;
    volatile uint32_t acknowledged;
    volatile uint32_t payload;
    volatile uint32_t response;
} frf_smp_job;
static inline void frf_smp_barrier(void)
{
#if defined(__arm__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
static inline uint32_t frf_smp_response(unsigned core, uint32_t payload)
{
    return payload ^ ((uint32_t)core * 0x9E3779B9u);
}
static inline void frf_smp_process_once(frf_smp_job *job, unsigned core)
{
    if (!job || core == 0u || core >= 4u) return;
    const uint32_t sequence = job->sequence;
    if (sequence == job->acknowledged) return;
    frf_smp_barrier(); /* Observe payload published before sequence. */
    const uint32_t payload = job->payload;
    job->response = frf_smp_response(core, payload);
    frf_smp_barrier(); /* Publish response before acknowledging. */
    job->acknowledged = sequence;
}
void frf_hal_smp_start_all(void);
int frf_hal_smp_ready(unsigned core);
int frf_hal_smp_ping(unsigned core, uint32_t value, uint32_t timeout_us);
void frf_hal_secondary_entry(unsigned core) __attribute__((noreturn));
#endif
