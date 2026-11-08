/*
 * FRF HAL BCM2837 ARM IRQ and system timer compare channel 1.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_IRQ_H
#define FRF_HAL_IRQ_H
#include <stdint.h>
extern volatile uint32_t frf_hal_irq_ticks;
extern volatile uint32_t frf_hal_irq_spurious;
void frf_hal_irq_timer_start(uint32_t period_us);
void frf_hal_irq_timer_stop(void);
#endif
