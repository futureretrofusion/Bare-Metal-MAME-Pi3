/*
 * FRF HAL early ARM32 platform interface (Pi 3 / BCM2837).
 * Copyright (c) 2026 Future Retro Fusion.
 * SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_STAGE0_H
#define FRF_HAL_STAGE0_H
#include <stdint.h>

/* Pi 3 ARM-side peripheral window, distinct from Pi 4 / Pi 5. */
#define FRF_BCM2837_PERIPHERAL_BASE 0x3F000000u
void frf_hal_uart_init(void);
void frf_hal_uart_putc(char c);
void frf_hal_uart_puts(const char *s);
void frf_hal_uart_hex32(uint32_t value);
uint64_t frf_hal_time_us(void);
void frf_hal_delay_us(uint32_t us);
void frf_hal_main(void) __attribute__((noreturn));
void frf_hal_irq_handler(void);
void frf_hal_exception_panic(uint32_t kind, uint32_t link,
                             uint32_t saved_psr) __attribute__((noreturn));
#endif
