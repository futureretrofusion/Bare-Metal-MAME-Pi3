/*
 * FRF HAL independent BCM2837 shared IRQ routing.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * GPU IRQ bank1 0..31, bank2 32..63, ARM-basic 64..71.
 * Basic bits 8..20 mirror GPU IRQ lines and are intentionally ignored.
 * This excludes GIC and ARM-local timer/mailbox interrupts; those are
 * separate controllers, not aliases of the legacy GPU IRQ.
 */
#include "hal.h"
#include "irq_router.h"
#include <stdint.h>
#if defined(__arm__)
#define FRF_IRQ_MASK_CPU() __asm__ volatile("cpsid i" ::: "memory")
#define FRF_IRQ_DMB() __asm__ volatile("dmb sy" ::: "memory")
#define FRF_IRQ_DSB() __asm__ volatile("dsb sy" ::: "memory")
#else
#define FRF_IRQ_MASK_CPU() ((void)0)
#define FRF_IRQ_DMB() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define FRF_IRQ_DSB() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif
#define BASE (FRF_BCM2837_PERIPHERAL_BASE + 0xB200u)
#define BASIC_PENDING (BASE + 0x00u)
#define PENDING1 (BASE + 0x04u)
#define PENDING2 (BASE + 0x08u)
#define ENABLE1 (BASE + 0x10u)
#define ENABLE2 (BASE + 0x14u)
#define ENABLE_BASIC (BASE + 0x18u)
#define DISABLE1 (BASE + 0x1Cu)
#define DISABLE2 (BASE + 0x20u)
#define DISABLE_BASIC (BASE + 0x24u)
#define SLOT_COUNT 72u
typedef struct { frf_hal_irq_callback fn; void *ctx; } slot_t;
static slot_t slots[SLOT_COUNT];
volatile uint32_t frf_hal_irq_dispatch_count;
volatile uint32_t frf_hal_irq_unhandled_count;
static uint32_t read32(uint32_t addr)
{ return *(volatile uint32_t *)(uintptr_t)addr; }
static void write32(uint32_t addr, uint32_t value)
{ *(volatile uint32_t *)(uintptr_t)addr = value; }

void frf_hal_irq_router_init(void)
{
    FRF_IRQ_MASK_CPU();
    write32(DISABLE1, 0xFFFFFFFFu);
    write32(DISABLE2, 0xFFFFFFFFu);
    write32(DISABLE_BASIC, 0xFFFFFFFFu);
    for (unsigned i = 0; i < SLOT_COUNT; ++i) {
        slots[i].fn = 0;
        slots[i].ctx = 0;
    }
    frf_hal_irq_dispatch_count = 0u;
    frf_hal_irq_unhandled_count = 0u;
    FRF_IRQ_DSB();
}
int frf_hal_irq_attach(unsigned irq, frf_hal_irq_callback fn, void *context)
{
    if (irq >= SLOT_COUNT || !fn) return 0;
    slots[irq].ctx = context;
    FRF_IRQ_DMB();
    slots[irq].fn = fn;
    return 1;
}
int frf_hal_irq_enable(unsigned irq)
{
    if (irq >= SLOT_COUNT || !slots[irq].fn) return 0;
    if (irq < 32u) write32(ENABLE1, 1u << irq);
    else if (irq < 64u) write32(ENABLE2, 1u << (irq - 32u));
    else write32(ENABLE_BASIC, 1u << (irq - 64u));
    FRF_IRQ_DSB();
    return 1;
}
int frf_hal_irq_disable(unsigned irq)
{
    if (irq >= SLOT_COUNT) return 0;
    if (irq < 32u) write32(DISABLE1, 1u << irq);
    else if (irq < 64u) write32(DISABLE2, 1u << (irq - 32u));
    else write32(DISABLE_BASIC, 1u << (irq - 64u));
    FRF_IRQ_DSB();
    return 1;
}
static void route_bank(unsigned base, uint32_t pending)
{
    while (pending) {
        /* Iterate pending bits; callback responsible for source ACK. */
        unsigned bit = (unsigned)__builtin_ctz(pending);
        unsigned irq = base + bit;
        pending &= pending - 1u;
        frf_hal_irq_callback fn = slots[irq].fn;
        if (fn) {
            fn(irq, slots[irq].ctx);
            ++frf_hal_irq_dispatch_count;
        } else {
            ++frf_hal_irq_unhandled_count;
#if defined(__arm__)
            /* Unhandled hardware source: mask just this IRQ,
               never interfere with the other interrupt banks. */
            (void)frf_hal_irq_disable(irq);
#endif
        }
    }
}
void frf_hal_irq_router_dispatch(uint32_t bank1, uint32_t bank2, uint32_t basic)
{
    route_bank(0u, bank1);
    route_bank(32u, bank2);
    route_bank(64u, basic & 0xFFu);
}
void frf_hal_irq_handler(void)
{
    frf_hal_irq_router_dispatch(read32(PENDING1),
                                read32(PENDING2),
                                read32(BASIC_PENDING));
    FRF_IRQ_DSB();
}
