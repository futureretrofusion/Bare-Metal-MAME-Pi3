/*
 * FRF independent Pi3 USB HID input interface, without Circle.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#ifndef FRF_HAL_INPUT_H
#define FRF_HAL_INPUT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void frf_hal_input_mount(uint8_t addr, uint8_t inst, uint16_t vid,
                         uint16_t pid, uint8_t protocol,
                         const uint8_t *descriptor, uint16_t len);
void frf_hal_input_unmount(uint8_t addr, uint8_t inst);
void frf_hal_input_report(uint8_t addr, uint8_t inst,
                          const uint8_t *report, uint16_t len);
/* [0] pad bits, [1] keyboard bits, [2] pad reports,
 * [3] keyboard reports, [4] pad decoding mode. */
void frf_hal_input_snapshot(uint32_t values[5]);
#ifdef __cplusplus
}
#endif
#endif
