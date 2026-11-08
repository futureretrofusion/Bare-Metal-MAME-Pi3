/*
 * Host-side IRQ bank dispatch tests using independent callback routing.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No live MMIO accesses in this host unit test.
 */
#include "../irq_router.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned visits[72];
static void fn(unsigned irq, void *ctx)
{
    assert(ctx == visits);
    visits[irq]++;
}
int main(void)
{
    for (unsigned i = 0u; i < 72u; ++i) {
        /* registration is independent of ARM MMIO */
        assert(frf_hal_irq_attach(i, fn, visits));
    }
    frf_hal_irq_router_dispatch((1u << 1) | (1u << 31),
                                (1u << 0) | (1u << 28),
                                (1u << 0) | (1u << 7) | (1u << 16));
    assert(visits[1] == 1u);
    assert(visits[31] == 1u);
    assert(visits[32] == 1u);
    assert(visits[60] == 1u);
    assert(visits[64] == 1u && visits[71] == 1u);
    assert(visits[67] == 0u);
    assert(frf_hal_irq_dispatch_count == 6u);
    assert(frf_hal_irq_unhandled_count == 0u);
    puts("FRF HAL shared IRQ router host tests: PASS");
    return 0;
}
