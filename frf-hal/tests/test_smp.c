/*
 * Host checks: one-producer-one-worker mailbox sequence and idempotency.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../smp.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    for (unsigned core = 1; core < 4; ++core) {
        frf_smp_job j = {0u, 0u, 0u, 0u};
        frf_smp_process_once(&j, core);
        assert(j.acknowledged == 0u && j.response == 0u);
        j.payload = 0xABCDEF12u;
        frf_smp_barrier();
        j.sequence = 1u;
        frf_smp_process_once(&j, core);
        assert(j.acknowledged == 1u);
        assert(j.response == frf_smp_response(core, 0xABCDEF12u));
        j.response = 0x12345678u; /* Duplicate must not execute. */
        frf_smp_process_once(&j, core);
        assert(j.response == 0x12345678u);
        /* Sequence arithmetic handles wrap (0xFFFFFFFF -> 0). */
        j.sequence = UINT32_MAX;
        j.acknowledged = UINT32_MAX;
        j.payload = 0xDEADBEEFu;
        j.sequence = 0u;
        frf_smp_process_once(&j, core);
        assert(j.acknowledged == 0u);
        assert(j.response == frf_smp_response(core, 0xDEADBEEFu));
    }
    puts("FRF HAL SMP protocol host tests: PASS");
    return 0;
}
