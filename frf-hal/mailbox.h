/*
 * FRF HAL VideoCore property mailbox API for Raspberry Pi 3.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_MAILBOX_H
#define FRF_HAL_MAILBOX_H
#include <stdint.h>
/* Message must be a 16-byte-aligned volatile static buffer.
   The call is single-core, non-reentrant and polled (no IRQ). */
int frf_hal_property_call(volatile uint32_t *message, unsigned words);
#endif
