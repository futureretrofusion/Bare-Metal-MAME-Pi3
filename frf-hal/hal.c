/*
 * FRF HAL: BCM2837 PL011 UART and 1 MHz system timer.
 * Copyright (c) 2026 Future Retro Fusion.
 * SPDX-License-Identifier: MIT
 *
 * Built from hardware register definitions; no Circle implementation used.
 * The firmware configuration must set init_uart_clock=48000000.
 */
#include "hal.h"
#include <stdint.h>

#define GPIO_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0x200000u)
#define GPFSEL1 (GPIO_BASE + 0x04u)
#define GPPUD (GPIO_BASE + 0x94u)
#define GPPUDCLK0 (GPIO_BASE + 0x98u)
#define PL011_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0x201000u)
#define UART_DR (PL011_BASE + 0x00u)
#define UART_FR (PL011_BASE + 0x18u)
#define UART_IBRD (PL011_BASE + 0x24u)
#define UART_FBRD (PL011_BASE + 0x28u)
#define UART_LCRH (PL011_BASE + 0x2Cu)
#define UART_CR (PL011_BASE + 0x30u)
#define UART_IMSC (PL011_BASE + 0x38u)
#define UART_ICR (PL011_BASE + 0x44u)
#define TIMER_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0x3000u)
#define TIMER_LO (TIMER_BASE + 0x04u)
#define TIMER_HI (TIMER_BASE + 0x08u)

static uint32_t mmio_read(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}
static void mmio_write(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}
static void gpio_wait(void)
{
    for (volatile unsigned n = 0; n < 180u; ++n) {
        __asm__ volatile("nop");
    }
}

void frf_hal_uart_init(void)
{
    mmio_write(UART_CR, 0u);
    uint32_t select = mmio_read(GPFSEL1);
    select &= ~((7u << 12) | (7u << 15));
    select |= (4u << 12) | (4u << 15); /* GPIO14/15 = ALT0 PL011 */
    mmio_write(GPFSEL1, select);
    mmio_write(GPPUD, 0u);
    gpio_wait();
    mmio_write(GPPUDCLK0, (1u << 14) | (1u << 15));
    gpio_wait();
    mmio_write(GPPUDCLK0, 0u);
    mmio_write(UART_ICR, 0x7ffu);
    mmio_write(UART_IMSC, 0u);
    /* 48 MHz PL011 clock: 115200 baud => IBRD=26, FBRD=3. */
    mmio_write(UART_IBRD, 26u);
    mmio_write(UART_FBRD, 3u);
    mmio_write(UART_LCRH, (3u << 5) | (1u << 4)); /* 8N1, FIFO */
    mmio_write(UART_CR, (1u << 0) | (1u << 8) | (1u << 9));
    __asm__ volatile("dsb sy" ::: "memory");
}
void frf_hal_uart_putc(char c)
{
    if (c == '\n') frf_hal_uart_putc('\r');
    /* Bounded wait prevents a stuck peripheral from hanging exception code. */
    unsigned limit = 1000000u;
    while ((mmio_read(UART_FR) & (1u << 5)) && --limit) {}
    if (limit) mmio_write(UART_DR, (uint32_t)(uint8_t)c);
}
void frf_hal_uart_puts(const char *s)
{
    while (s && *s) frf_hal_uart_putc(*s++);
}
void frf_hal_uart_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    frf_hal_uart_puts("0x");
    for (unsigned i = 0; i != 8u; ++i) {
        unsigned shift = 28u - i * 4u;
        frf_hal_uart_putc(hex[(value >> shift) & 15u]);
    }
}
uint64_t frf_hal_time_us(void)
{
    uint32_t hi0, lo, hi1;
    do {
        hi0 = mmio_read(TIMER_HI);
        lo = mmio_read(TIMER_LO);
        hi1 = mmio_read(TIMER_HI);
    } while (hi0 != hi1);
    return ((uint64_t)hi0 << 32) | lo;
}
void frf_hal_delay_us(uint32_t delay)
{
    /* Unsigned subtraction handles a 32-bit CLO wrap for short waits. */
    uint32_t start = mmio_read(TIMER_LO);
    while ((uint32_t)(mmio_read(TIMER_LO) - start) < delay) {}
}
void frf_hal_exception_panic(uint32_t kind, uint32_t link, uint32_t saved_psr)
{
    frf_hal_uart_puts("\nFRF HAL EXCEPTION kind=");
    frf_hal_uart_hex32(kind);
    frf_hal_uart_puts(" LR=");
    frf_hal_uart_hex32(link);
    frf_hal_uart_puts(" SPSR=");
    frf_hal_uart_hex32(saved_psr);
    frf_hal_uart_puts("\nHALTED\n");
    for (;;) __asm__ volatile("wfe");
}
