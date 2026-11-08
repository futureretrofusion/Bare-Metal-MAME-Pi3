/*
 * FRF HAL original project HID adapter: descriptor/known-pad decode.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Uses original FRF generic descriptor parser; GPL-derived known-pad code excluded.
 * No Circle, Circle-stdlib, std:: library, dynamic allocation or Linux.
 */
#include "input.h"
#include "../baremetal/bm_platform.h"
#include "../baremetal/tinyusb/frf_hid_gamepad.h"
#include <stdint.h>
#include <string.h>

static struct Device {
    uint8_t addr, inst, mode;
    uint8_t report_id;
    uint16_t vid, pid;
    frf_hid::Layout layout;
} pad;
static uint8_t keyboard_addr, keyboard_inst;
static volatile uint32_t pad_bits, key_bits, pad_reports, key_reports;

extern "C" void frf_hal_input_mount(uint8_t addr, uint8_t inst, uint16_t vid,
                                     uint16_t pid, uint8_t protocol,
                                     const uint8_t *desc, uint16_t len)
{
    /* Vendor 057e:2009 and 2563 controllers can advertise keyboard HID.
       Never treat these vendor IDs as a keyboard; they require a valid
       independently parsed report descriptor for pad use. */
    const bool known_pad_id = (vid == 0x057eu && pid == 0x2009u) ||
                              (vid == 0x2563u && (pid == 0x0575u || pid == 0x0576u));
    if (protocol == 1u && !known_pad_id) {
        if (keyboard_addr == 0u) {
            keyboard_addr = addr; keyboard_inst = inst;
            key_bits = 0u;
        }
        return;
    }
    if (pad.addr != 0u) return;
    frf_hid::Layout candidate = {};
    const bool parsed = frf_hid::parse(desc, len, &candidate);
    if (!parsed) return;
    pad.addr = addr;
    pad.inst = inst;
    pad.vid = vid;
    pad.pid = pid;
    pad.report_id = parsed ? candidate.report_id : 0u;
    pad.layout = candidate;
    pad.mode = 1u;
    pad_bits = 0u;
}
extern "C" void frf_hal_input_unmount(uint8_t addr, uint8_t inst)
{
    if (pad.addr == addr && pad.inst == inst) {
        memset(&pad, 0, sizeof(pad));
        pad_bits = 0u;
    }
    if (keyboard_addr == addr && keyboard_inst == inst) {
        keyboard_addr = 0u; keyboard_inst = 0u;
        key_bits = 0u;
    }
}
static uint32_t keyboard_decode(const uint8_t *r, uint16_t n)
{
    if (!r || n < 8u) return 0u;
    uint32_t mask = 0u;
    for (unsigned i = 2; i < 8u; ++i) {
        switch(r[i]) {
            case 0x50u: mask |= BM_PAD_LEFT; break;
            case 0x4fu: mask |= BM_PAD_RIGHT; break;
            case 0x52u: mask |= BM_PAD_UP; break;
            case 0x51u: mask |= BM_PAD_DOWN; break;
            case 0x2cu: mask |= BM_PAD_B1; break; /* space */
            case 0x28u: mask |= BM_PAD_START; break; /* enter */
            case 0x29u: mask |= BM_PAD_SELECT; break; /* escape */
            default: break;
        }
    }
    return mask;
}
extern "C" void frf_hal_input_report(uint8_t addr, uint8_t inst,
                                      const uint8_t *report, uint16_t len)
{
    if (!report) return;
    if (addr == keyboard_addr && inst == keyboard_inst) {
        if (len >= 8u) {
            key_bits = keyboard_decode(report, len);
            ++key_reports;
        }
        return;
    }
    if (addr != pad.addr || inst != pad.inst) return;
    uint32_t mask = 0u;
    bool valid = false;
    if (pad.layout.valid &&
        (!pad.report_id || (len && report[0] == pad.report_id))) {
        /* Do not clear controls on unrelated report IDs. */
        mask = frf_hid::decode(pad.layout, report, len);
        valid = true;
    }
    if (valid) {
        pad_bits = mask;
        ++pad_reports;
    }
}
extern "C" void frf_hal_input_snapshot(uint32_t values[5])
{
    if (!values) return;
    values[0] = pad_bits;
    values[1] = key_bits;
    values[2] = pad_reports;
    values[3] = key_reports;
    values[4] = pad.mode;
}
