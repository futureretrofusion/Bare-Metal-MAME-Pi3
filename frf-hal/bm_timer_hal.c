/*
 * FRF HAL adapter for original MAME renderer's 1MHz timing contract.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <stdint.h>
uint64_t bm_cycles(void) { return frf_hal_time_us(); }
uint64_t bm_cycles_per_second(void) { return 1000000ull; }
