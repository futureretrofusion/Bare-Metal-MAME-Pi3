/*
 * FRF HAL: independent Pi3 TinyUSB host enumeration/USB HID diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * The diagnostic does not link Circle, Circle-stdlib or MAME.
 * It polls the MIT TinyUSB BCM2837 DWC2 host with MMU/D-cache disabled.
 * It reports USB mount and HID report events only (not full gamepad mapping).
 * Hardware validation and testing on a separate SD are still required.
 */
#include "hal.h"
#include "usb_power.h"
#include <stdint.h>
#include <stddef.h>
#include "tusb.h"
#include "host/hcd.h"
#include "class/hid/hid_host.h"

static volatile uint32_t mount_count;
static volatile uint32_t live_devices;
static volatile uint32_t unmount_count;
static volatile uint32_t hid_mount_count;
static volatile uint32_t hid_report_count;
static volatile uint32_t enum_errors;
static volatile uint32_t last_vidpid;
static volatile uint32_t last_report_word;
static volatile uint32_t last_report_size;

volatile uint8_t frf_r26_receiver_addr;
volatile uint8_t frf_r32_receiver_addr;
void frf_pi3_trace_control_length(uint16_t wire, uint16_t cpu)
{ (void)wire; (void)cpu; }
void frf_pi3_trace_ep(uint32_t a, uint32_t b, uint32_t c)
{ (void)a; (void)b; (void)c; }
void frf_pi3_trace_xfer(uint32_t a, uint32_t b, uint32_t c,
                       uint32_t d, uint32_t e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
void frf_tinyusb_enum_observe(uint32_t state, uint32_t result)
{ (void)state; if (result != 0u) ++enum_errors; }

uint32_t tusb_time_millis_api(void)
{ return (uint32_t)(frf_hal_time_us() / 1000u); }

/* BCM2837 ARM physical RAM below 1GiB is visible to DWC2 at 0xC0000000.
 * This diagnostic runs with D-cache disabled; no cache state is assumed.
 * A subsequent cached/multicore port must supply real cache maintenance.
 */
void *tusb_app_virt_to_phys(void *ptr)
{
    const uintptr_t a = (uintptr_t)ptr;
    if (!ptr || a >= 0x40000000u) return NULL;
    return (void *)(uintptr_t)(a | 0xC0000000u);
}
void arm_flush_cache(uintptr_t addr, uint32_t size)
{ (void)addr; (void)size; __asm__ volatile("dsb sy" ::: "memory"); }
void arm_dcache_invalidate(uintptr_t addr, uint32_t size)
{ (void)addr; (void)size; __asm__ volatile("dsb sy" ::: "memory"); }
void tusb_app_dcache_flush(uintptr_t addr, uint32_t size)
{ arm_flush_cache(addr, size); }
void tusb_app_dcache_invalidate(uintptr_t addr, uint32_t size)
{ arm_dcache_invalidate(addr, size); }

/* DWC2 FIFO registers must be accessed in whole 32-bit words. */
void tu_hwfifo_write(volatile void *fifo, const uint8_t *src, uint16_t n,
                     const tu_hwfifo_access_t *access)
{
    (void)access;
    for (uint32_t i = 0; i < n; i += 4u) {
        uint32_t value = 0u;
        for (uint32_t j = 0; j < 4u && i + j < n; ++j)
            value |= (uint32_t)src[i + j] << (8u * j);
        *(volatile uint32_t *)fifo = value;
    }
}
void tu_hwfifo_read(const volatile void *fifo, uint8_t *dest, uint16_t n,
                    const tu_hwfifo_access_t *access)
{
    (void)access;
    for (uint32_t i = 0; i < n; i += 4u) {
        uint32_t value = *(const volatile uint32_t *)fifo;
        for (uint32_t j = 0; j < 4u && i + j < n; ++j)
            dest[i + j] = (uint8_t)(value >> (8u * j));
    }
}
void tuh_mount_cb(uint8_t address)
{
    uint16_t vid = 0u, pid = 0u;
    tuh_vid_pid_get(address, &vid, &pid);
    last_vidpid = ((uint32_t)vid << 16u) | pid;
    ++mount_count;
    ++live_devices;
    frf_hal_uart_puts("USB device mounted, VID:PID=");
    frf_hal_uart_hex32(last_vidpid);
    frf_hal_uart_puts("\n");
}
void tuh_umount_cb(uint8_t address)
{
    (void)address;
    ++unmount_count;
    if (live_devices) --live_devices;
    frf_hal_uart_puts("USB device removed\n");
}
void tuh_hid_mount_cb(uint8_t address, uint8_t instance,
                       const uint8_t *desc, uint16_t len)
{
    (void)desc; (void)len;
    ++hid_mount_count;
    frf_hal_uart_puts("HID interface mounted\n");
    (void)tuh_hid_receive_report(address, instance);
}
void tuh_hid_umount_cb(uint8_t address, uint8_t instance)
{ (void)address; (void)instance; }
void tuh_hid_report_received_cb(uint8_t address, uint8_t instance,
                                 const uint8_t *data, uint16_t len)
{
    uint32_t word = 0u;
    uint16_t cap = len < 4u ? len : 4u;
    for (uint16_t i = 0; i < cap; ++i)
        word |= (uint32_t)data[i] << (8u * i);
    last_report_word = word;
    last_report_size = len;
    ++hid_report_count;
    (void)tuh_hid_receive_report(address, instance);
}
static void dump_status(void)
{
    frf_hal_uart_puts("mount=");
    frf_hal_uart_hex32(mount_count);
    frf_hal_uart_puts(" live=");
    frf_hal_uart_hex32(live_devices);
    frf_hal_uart_puts(" unmount=");
    frf_hal_uart_hex32(unmount_count);
    frf_hal_uart_puts(" hid=");
    frf_hal_uart_hex32(hid_mount_count);
    frf_hal_uart_puts(" reports=");
    frf_hal_uart_hex32(hid_report_count);
    frf_hal_uart_puts(" errors=");
    frf_hal_uart_hex32(enum_errors);
    frf_hal_uart_puts(" size=");
    frf_hal_uart_hex32(last_report_size);
    frf_hal_uart_puts(" first4=");
    frf_hal_uart_hex32(last_report_word);
    frf_hal_uart_puts("\n");
}

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / Circle-free TinyUSB host diagnostic\n");
    uint32_t power_state = 0u;
    if (!frf_hal_usb_hcd_power_on(&power_state)) {
        frf_hal_uart_puts("USB firmware power ON FAILED, state=");
        frf_hal_uart_hex32(power_state);
        frf_hal_uart_puts("\n");
        for (;;) __asm__ volatile("wfe");
    }
    frf_hal_delay_us(100000u);
    const uint32_t snpsid = frf_hal_dwc2_core_id();
    frf_hal_uart_puts("USB DWC2 SNPSID=");
    frf_hal_uart_hex32(snpsid);
    frf_hal_uart_puts("\n");
    if ((snpsid & 0xFFFF0000u) != 0x4F540000u) {
        frf_hal_uart_puts("USB core ID invalid, not starting HCD\n");
        for (;;) __asm__ volatile("wfe");
    }

    const tusb_rhport_init_t cfg = {TUSB_ROLE_HOST, TUSB_SPEED_HIGH};
    if (!tuh_rhport_init(0u, &cfg)) {
        frf_hal_uart_puts("TinyUSB host initialization FAILED\n");
        for (;;) __asm__ volatile("wfe");
    }
    frf_hal_uart_puts("TinyUSB host initialized (hardware enumeration NOT proven)\n");
    bool previously_connected = hcd_port_connect_status(0u);
    if (previously_connected) hcd_event_device_attach(0u, false);
    uint32_t last_dump = tusb_time_millis_api();
    uint32_t last_attach = last_dump;
    for (;;) {
        hcd_int_handler(0u, false);
        tuh_task_ext(0u, false);
        const uint32_t now = tusb_time_millis_api();
        const bool connected = hcd_port_connect_status(0u);
        if (!previously_connected && connected)
            hcd_event_device_attach(0u, false);
        if (connected && live_devices == 0u &&
            (uint32_t)(now - last_attach) >= 1500u) {
            hcd_event_device_attach(0u, false);
            last_attach = now;
        }
        previously_connected = connected;
        if ((uint32_t)(now - last_dump) >= 1000u) {
            dump_status();
            last_dump = now;
        }
        __asm__ volatile("yield" ::: "memory");
    }
}
