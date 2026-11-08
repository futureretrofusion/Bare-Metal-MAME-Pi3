/*
 * FRF HAL Pi3 isolated USB HCD power diagnostic; NO TinyUSB linked.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "usb_power.h"
#include <stdint.h>
void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / independent USB power test\n");
    uint32_t state = 0xFFFFFFFFu;
    int ok = frf_hal_usb_hcd_power_on(&state);
    frf_hal_uart_puts("USB HCD power ACK: ");
    frf_hal_uart_puts(ok ? "YES\n" : "NO\n");
    frf_hal_uart_puts("USB HCD response state: ");
    frf_hal_uart_hex32(state);
    frf_hal_uart_puts("\n");
    if (ok) {
        frf_hal_delay_us(100000u);
        uint32_t id = frf_hal_dwc2_core_id();
        frf_hal_uart_puts("DWC2 SNPSID: ");
        frf_hal_uart_hex32(id);
        frf_hal_uart_puts("\n");
        frf_hal_uart_puts((id & 0xFFFF0000u) == 0x4F540000u ?
            "DWC2 core ID: PASS\n" : "DWC2 core ID: CHECK HARDWARE\n");
        frf_hal_uart_puts("DWC2 HPRT: ");
        frf_hal_uart_hex32(frf_hal_dwc2_port_register());
        frf_hal_uart_puts("\n");
    }
    frf_hal_uart_puts("USB power diagnostic complete. No HID started.\n");
    for (;;) {
        frf_hal_delay_us(1000000u);
        frf_hal_uart_puts("USBPOWER HEARTBEAT\n");
    }
}
