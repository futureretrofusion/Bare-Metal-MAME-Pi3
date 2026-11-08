/*
 * FRF HAL standalone GUI integration test, 1280x720 RGB565.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Renders the EXISTING FRF menu code with original assets, no ROM data.
 */
#include "hal.h"
#include "framebuffer.h"
#include "../circle-kernel/frf_gui.h"
#include <stdint.h>

static frf_hal_framebuffer display;
static int frf_gui_page_flip(unsigned page)
{
    return frf_hal_fb_flip(&display,page);
}
static void halted_message(const char *s)
{
    frf_hal_uart_puts(s);
    for (;;) {
        frf_hal_delay_us(1000000u);
        frf_hal_uart_puts("GUI framebuffer unavailable\n");
    }
}
static const char *const names[14] = {
    "Future Retro Fusion", "Independent ARM32 HAL",
    "MAME 0.106 frontend", "RGB565 double-buffer test",
    "Integer scaling and scanlines", "TinyUSB HID integration pending",
    "SD / ROM reader pending", "HDMI audio pending",
    "Multicore work pending", "No copyrighted ROMs bundled",
    "Original FRF artwork", "Preserved game drivers",
    "Independent Circle-free path", "Development build only"
};

void frf_hal_main(void)
{
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / existing FRF graphical menu\n");
    if(!frf_hal_fb_open(&display,1280u,720u))
        halted_message("1280x720 RGB565 allocation failed\n");
    frf_gui_attach((void *)(uintptr_t)display.physical_address,
                   display.width, display.height,
                   display.pitch_bytes, 16u);
    frf_gui_configure_double(display.virtual_height,display.buffer_bytes,
                             frf_gui_page_flip);
    if (!frf_gui_available())
        halted_message("Existing FRF GUI not available on requested mode\n");
    frf_gui_model m={0};
    m.count=14u;
    m.total_count=14u;
    m.rows=14u;
    m.native_width=1280u;
    m.native_height=720u;
    m.native_millihz=60000u;
    m.selected_short="FRF-HAL";
    m.selected_title="Independent kernel rendering the existing FRF menu";
    m.selected_year="2026";
    m.selected_maker="Future Retro Fusion";
    m.last_message="Independent ARM32 frontend test - NO GAMES OR ROMS";
    m.usb_host_ok=0u;
    m.double_buffer=(unsigned)frf_gui_is_double();
    for (unsigned i=0;i<14u;i++) {
        m.items[i].title=names[i];
        m.items[i].short_name="frf-test";
        m.items[i].status=0u;
        m.items[i].rom_status=0u;
    }
    unsigned selected=0u;
    for (;;) {
        m.selected=selected;
        m.selected_title=names[selected];
        frf_gui_render(&m);
        frf_hal_delay_us(300000u);
        selected=(selected+1u)%14u;
    }
}
