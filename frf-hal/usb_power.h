/*
 * FRF HAL Raspberry Pi 3 firmware-controlled DWC2 power.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_USB_POWER_H
#define FRF_HAL_USB_POWER_H
#include <stdint.h>
/* Returns 1 only after firmware acknowledges USB HCD power on.
 * Not thread safe; single-core mailbox bring-up use only. */
int frf_hal_usb_hcd_power_on(uint32_t *state_out);
uint32_t frf_hal_dwc2_core_id(void);
uint32_t frf_hal_dwc2_port_register(void);
#endif
