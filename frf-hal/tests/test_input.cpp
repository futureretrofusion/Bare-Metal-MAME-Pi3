/*
 * FRF HAL host unit tests for independently integrated FRF HID mappings.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../input.h"
#include "../../baremetal/bm_platform.h"
#include <assert.h>
#include <stdio.h>
static uint32_t s[5];
int main()
{
    const uint8_t boot_keyboard[8] = {0,0,0x52,0x2C,0,0,0,0};
    frf_hal_input_mount(1,0,0x046D,0xC31C,1,nullptr,0);
    frf_hal_input_report(1,0,boot_keyboard,sizeof boot_keyboard);
    frf_hal_input_snapshot(s);
    assert((s[1] & (BM_PAD_UP | BM_PAD_B1)) == (BM_PAD_UP | BM_PAD_B1));
    assert(s[3] == 1u);
    frf_hal_input_unmount(1,0);
    frf_hal_input_snapshot(s);
    assert(s[1] == 0u);
    const uint8_t descriptor[] = {
       0x05,0x01, 0x09,0x05, 0xA1,0x01, 0x15,0x00,
       0x26,0xFF,0x00, 0x75,0x08, 0x95,0x02,
       0x09,0x30, 0x09,0x31, 0x81,0x02,
       0x05,0x09, 0x19,0x01, 0x29,0x02,
       0x15,0x00, 0x25,0x01, 0x75,0x01,
       0x95,0x02, 0x81,0x02, 0x75,0x06,
       0x95,0x01, 0x81,0x03, 0xC0
    };
    const uint8_t report[] = {0,128,1};
    frf_hal_input_mount(3,0,0x1234,0x5678,0,descriptor,sizeof descriptor);
    frf_hal_input_report(3,0,report,sizeof report);
    frf_hal_input_snapshot(s);
    assert(s[4] == 1u);
    assert((s[0] & (BM_PAD_LEFT | BM_PAD_B1)) == (BM_PAD_LEFT | BM_PAD_B1));
    frf_hal_input_unmount(3,0);

    /* A special-case controller with no standard HID descriptor
       is deliberately not claimed until clean provenance is established. */
    frf_hal_input_mount(4,0,0x057E,0x2009,0,nullptr,0);
    frf_hal_input_snapshot(s);
    assert(s[4] == 0u);
    puts("FRF HAL HID input decoding host tests: PASS");
    return 0;
}
