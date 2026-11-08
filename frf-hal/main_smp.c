/*
 * FRF HAL Pi3 independent multicore handoff/IPC diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "smp.h"
#include <stdint.h>
void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / multicore + mailbox3 diagnostic\n");
    frf_hal_smp_start_all();
    uint32_t failures = 0u;
    for (uint32_t core = 1u; core < 4u; ++core) {
        const uint64_t t0 = frf_hal_time_us();
        while (!frf_hal_smp_ready(core) &&
               frf_hal_time_us() - t0 < 1000000ull) {}
        frf_hal_uart_puts("Core ");
        frf_hal_uart_hex32(core);
        if (!frf_hal_smp_ready(core)) {
            frf_hal_uart_puts(" start TIMEOUT\n");
            ++failures;
            continue;
        }
        frf_hal_uart_puts(" started; IPC ");
        if (frf_hal_smp_ping(core, 0x12345678u + core, 250000u))
            frf_hal_uart_puts("PASS\n");
        else {
            frf_hal_uart_puts("FAIL\n");
            ++failures;
        }
    }
    frf_hal_uart_puts(failures ? "SMP START/IPC: FAIL\n" :
                                 "SMP START/IPC: PASS\n");
    uint32_t count = 0u;
    for (;;) {
        frf_hal_delay_us(1000000u);
        ++count;
        for (unsigned core = 1; core < 4; ++core) {
            if (frf_hal_smp_ready(core) &&
                !frf_hal_smp_ping(core, count ^ 0xABCDEF01u, 100000u))
                frf_hal_uart_puts("SMP periodic IPC FAIL\n");
        }
        frf_hal_uart_puts("SMP HEARTBEAT\n");
    }
}
