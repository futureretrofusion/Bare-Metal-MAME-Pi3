/*
 * FRF HAL independent USB HCD power control, BCM2837/Raspberry Pi 3.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Hardware protocol: Raspberry Pi firmware property interface.
 * Not derived from Circle/Circle-stdlib source implementations.
 */
#include "usb_power.h"
#include "hal.h"
#include "mailbox.h"
#include <stdint.h>

#define FRF_POWER_TAG_SET 0x00028001u
#define FRF_POWER_DEVICE_USB_HCD 3u
#define FRF_POWER_STATE_ON 1u
#define FRF_POWER_STATE_WAIT 2u
#define FRF_DWC2_BASE (FRF_BCM2837_PERIPHERAL_BASE + 0x980000u)
#define FRF_DWC2_CORE_ID (FRF_DWC2_BASE + 0x40u)
#define FRF_DWC2_PORT (FRF_DWC2_BASE + 0x440u)

static volatile uint32_t usb_request[8] __attribute__((aligned(16)));

int frf_hal_usb_hcd_power_on(uint32_t *state_out)
{
    volatile uint32_t *p = usb_request;
    p[0] = 8u * sizeof(uint32_t);
    p[1] = 0u;
    p[2] = FRF_POWER_TAG_SET;
    p[3] = 8u;
    p[4] = 8u;
    p[5] = FRF_POWER_DEVICE_USB_HCD;
    p[6] = FRF_POWER_STATE_ON | FRF_POWER_STATE_WAIT;
    p[7] = 0u;
    if (!frf_hal_property_call(p, 8u)) return 0;
    if ((p[4] & 0x80000000u) == 0u ||
        (p[4] & 0x7FFFFFFFu) < 8u ||
        p[5] != FRF_POWER_DEVICE_USB_HCD) return 0;
    if (state_out) *state_out = p[6];
    /* Bit 1 means device does NOT exist in the response. */
    return ((p[6] & 3u) == FRF_POWER_STATE_ON);
}
uint32_t frf_hal_dwc2_core_id(void)
{
    return *(volatile uint32_t *)(uintptr_t)FRF_DWC2_CORE_ID;
}
uint32_t frf_hal_dwc2_port_register(void)
{
    return *(volatile uint32_t *)(uintptr_t)FRF_DWC2_PORT;
}
