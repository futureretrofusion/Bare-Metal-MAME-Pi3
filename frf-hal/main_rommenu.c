/*
 * Future Retro Fusion independent Pi3 ROM directory menu diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Reuses the original FRF graphics and reads *names only* from a FAT32
 * card through the isolated SDHOST driver. This is not a MAME launcher.
 * No game ROM payload is read or embedded. No card data is written.
 */
#include "hal.h"
#include "framebuffer.h"
#include "sdhost.h"
#include "fat32.h"
#include "../circle-kernel/frf_gui.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FRF_ROM_DISPLAY_LIMIT FRF_GUI_ROWS
#define FRF_ROM_SCAN_CAP 8192u
#define FRF_ROM_NAME_LIMIT 95u
static frf_hal_framebuffer video;
static frf_sdhost sd;
static frf_fat32 fs;
static frf_gui_model model;
static char visible[FRF_ROM_DISPLAY_LIMIT][FRF_ROM_NAME_LIMIT + 1u];
static uint32_t rom_count;
static const char *result_line="SD not initialized";
static unsigned init_ok;
static int present(unsigned page) {
    return frf_hal_fb_flip(&video,page);
}
static int is_zip_name(const char *name) {
    if (!name) return 0;
    size_t n=strlen(name);
    if (n < 5u) return 0;
    const char *e=name+n-4u;
    if (e[0]!='.') return 0;
    return (e[1]=='z'||e[1]=='Z') &&
           (e[2]=='i'||e[2]=='I') &&
           (e[3]=='p'||e[3]=='P');
}
static int rom_item(const frf_fat32_entry *item,void *context) {
    (void)context;
    if ((item->attributes & 0x10u) || !is_zip_name(item->name))return 0;
    if (rom_count < FRF_ROM_DISPLAY_LIMIT) {
        char *dest=visible[rom_count];
        unsigned i=0;
        while(i<FRF_ROM_NAME_LIMIT && item->name[i]){
            dest[i]=item->name[i];
            ++i;
        }
        dest[i]=0;
    }
    if (++rom_count == FRF_ROM_SCAN_CAP) return 1;
    return 0;
}
static void update_model(unsigned selected) {
    model.count=rom_count;
    model.total_count=rom_count;
    model.first=0u;
    model.rows=rom_count<FRF_ROM_DISPLAY_LIMIT ? rom_count : FRF_ROM_DISPLAY_LIMIT;
    model.selected=selected;
    model.native_width=1280u;
    model.native_height=720u;
    model.native_millihz=60000u;
    model.double_buffer=frf_gui_is_double();
    model.selected_year="2026";
    model.selected_maker="Future Retro Fusion";
    model.last_message=result_line;
    model.usb_host_ok=0u;
    for(unsigned i=0u;i<model.rows;++i) {
        model.items[i].title=visible[i];
        model.items[i].short_name=visible[i];
        model.items[i].status=0u;
        model.items[i].rom_status=0u;
    }
    if(model.rows) {
        model.selected_title=visible[selected];
        model.selected_short=visible[selected];
    }else{
        model.selected_title="Read-only SD ROM listing unavailable";
        model.selected_short="FRF HAL DIAGNOSTIC";
    }
}
void frf_hal_main(void) {
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL Pi3 - FAT32 + ORIGINAL FRF menu diagnostic\n");
    if(!frf_hal_fb_open(&video,1280u,720u)) {
        frf_hal_uart_puts("720p framebuffer allocation FAILED\n");
        for(;;) __asm__ volatile("wfe");
    }
    frf_gui_attach((void *)(uintptr_t)video.physical_address,
                    video.width,video.height,video.pitch_bytes,16u);
    frf_gui_configure_double(video.virtual_height,video.buffer_bytes,present);
    if(!frf_gui_available()){
        frf_hal_uart_puts("FRF renderer unavailable\n");
        for(;;) __asm__ volatile("wfe");
    }
    update_model(0u);
    frf_gui_render(&model);
    frf_hal_uart_puts("Starting SDHOST read-only, no writes.\n");
    init_ok=(unsigned)frf_sdhost_initialize(&sd);
    if(!init_ok) {
        result_line="SDHOST initialization failed - see UART diagnostics";
        frf_hal_uart_puts("SDHOST init FAILED: CMD=");
        frf_hal_uart_hex32(sd.last_command);
        frf_hal_uart_puts(" HSTS=");
        frf_hal_uart_hex32(sd.last_status);
        frf_hal_uart_puts("\n");
    } else {
        int s=frf_fat32_mount(&fs,frf_sdhost_fat_read,&sd);
        if(s!=FRF_FAT_OK) {
            result_line="Card boot partition is not supported FAT32";
            frf_hal_uart_puts("FAT32 mount failed: ");
            frf_hal_uart_hex32((uint32_t)s);
            frf_hal_uart_puts("\n");
        } else {
            s=frf_fat32_list(&fs,"/roms",rom_item,0);
            if(s==FRF_FAT_OK) result_line="FAT32 /roms ZIP names only - no games launched";
            else result_line="/roms directory missing or unreadable";
            frf_hal_uart_puts("ROM directory status: ");
            frf_hal_uart_hex32((uint32_t)s);
            frf_hal_uart_puts(" ZIPs scanned: ");
            frf_hal_uart_hex32(rom_count);
            frf_hal_uart_puts("\n");
        }
    }
    unsigned selected=0u;
    for (;;) {
        if(model.rows && selected>=model.rows) selected=0u;
        update_model(selected);
        frf_gui_render(&model);
        frf_hal_delay_us(350000u);
        if(model.rows) selected=(selected+1u)%model.rows;
    }
}
