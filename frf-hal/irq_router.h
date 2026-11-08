/*
 * FRF HAL shared ARM IRQ dispatcher, BCM2837 legacy GPU controller.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_IRQ_ROUTER_H
#define FRF_HAL_IRQ_ROUTER_H
#include <stdint.h>
typedef void (*frf_hal_irq_callback)(unsigned irq, void *context);
void frf_hal_irq_router_init(void);
int frf_hal_irq_attach(unsigned irq, frf_hal_irq_callback fn, void *context);
int frf_hal_irq_enable(unsigned irq);
int frf_hal_irq_disable(unsigned irq);
void frf_hal_irq_router_dispatch(uint32_t bank1, uint32_t bank2, uint32_t basic);
extern volatile uint32_t frf_hal_irq_dispatch_count;
extern volatile uint32_t frf_hal_irq_unhandled_count;
#endif
