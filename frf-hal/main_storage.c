/*
 * Future Retro Fusion — isolated Pi3 SDHOST + FAT32 read-only diagnostic.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * No games, original MAME or Circle runtime are linked. This image
 * sends only SD card read commands; no filesystem or card writes.
 * Hardware test has NOT been performed on a physical Raspberry Pi.
 */
#include "hal.h"
#include "sdhost.h"
#include "fat32.h"
#include "rom_catalog.h"
#include "file_pool.h"
#include <stdint.h>
static frf_sdhost sd;
static frf_fat32 fs;
static uint8_t boot_sector[512];
/* A small provisional index to test integration, NOT full MAME drivers. */
static const frf_rom_driver pilot_drivers[] = {
    {"galaga", "Galaga"}, {"pacman", "Pac-Man"},
    {"mspacman", "Ms. Pac-Man"}, {"pacland", "Pac-Land"},
    {"outrun", "Out Run"}, {"commando", "Commando"},
    {"sf2", "Street Fighter II"}
};
static frf_rom_catalog pilot_index;
static frf_file_pool files;

typedef struct {uint32_t total;uint32_t limit;} listing;
static int print_entry(const frf_fat32_entry *e,void *p){
    listing *l=(listing *)p;
    ++l->total;
    if(l->total<=l->limit){
        frf_hal_uart_puts((e->attributes & 0x10u)?"DIR  ":"FILE ");
        frf_hal_uart_puts(e->name);
        frf_hal_uart_puts(" size=");
        frf_hal_uart_hex32(e->size);
        frf_hal_uart_puts(" cluster=");
        frf_hal_uart_hex32(e->first_cluster);
        frf_hal_uart_puts("\n");
    }
    return l->total >= 1000u;
}
static void print_error(const char *name) {
    frf_hal_uart_puts(name);
    frf_hal_uart_puts(" CMD=");
    frf_hal_uart_hex32(sd.last_command);
    frf_hal_uart_puts(" WORD=");
    frf_hal_uart_hex32(sd.last_command_word);
    frf_hal_uart_puts(" STATUS=");
    frf_hal_uart_hex32(sd.last_status);
    frf_hal_uart_puts(" EDM=");
    frf_hal_uart_hex32(sd.last_edm);
    frf_hal_uart_puts(" RESP=");
    frf_hal_uart_hex32(sd.last_response);
    frf_hal_uart_puts("\n");
}
void frf_hal_main(void) {
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL Pi3 SDHOST/FAT32 READ-ONLY diagnostic\n");
    frf_hal_uart_puts("SDHOST 0x3F202000, NOT Arasan 0x3F300000\n");
    frf_sdhost_probe(&sd);
    print_error("Pre-init SDHOST registers:");
    if(!frf_sdhost_initialize(&sd)){
        print_error("SD card initialization FAILED:");
        frf_hal_uart_puts("Check Pi3 GPIO48-53 pin mux, controller route and SD clock.\n");
        for(;;) __asm__ volatile("wfe");
    }
    frf_hal_uart_puts("SD init returned success; RCA=");
    frf_hal_uart_hex32(sd.rca);
    frf_hal_uart_puts(" OCR=");
    frf_hal_uart_hex32(sd.ocr);
    frf_hal_uart_puts(sd.high_capacity?" SDHC/SDXC\n":" SDSC\n");
    if(!frf_sdhost_read_sector(&sd,0u,boot_sector)){
        print_error("LBA0 read FAILED:");
        for(;;) __asm__ volatile("wfe");
    }
    frf_hal_uart_puts("LBA0 signature=");
    frf_hal_uart_hex32((uint32_t)boot_sector[510] |
                         ((uint32_t)boot_sector[511]<<8u));
    frf_hal_uart_puts(" sectors_read=");
    frf_hal_uart_hex32(sd.sectors_read);
    frf_hal_uart_puts("\n");
    int status=frf_fat32_mount(&fs,frf_sdhost_fat_read,&sd);
    if(status!=FRF_FAT_OK){
        frf_hal_uart_puts("FAT32 mount NOT AVAILABLE, status=");
        frf_hal_uart_hex32((uint32_t)status);
        frf_hal_uart_puts(" (FAT12/16/exFAT unsupported)\n");
        for(;;) __asm__ volatile("wfe");
    }
    frf_hal_uart_puts("FAT32 mounted, volume_lba=");
    frf_hal_uart_hex32(fs.volume_lba);
    frf_hal_uart_puts(" root_cluster=");
    frf_hal_uart_hex32(fs.root_cluster);
    frf_hal_uart_puts("\n");
    listing list={0u,20u};
    status=frf_fat32_list(&fs,"/",print_entry,&list);
    frf_hal_uart_puts("Root directory return=");
    frf_hal_uart_hex32((uint32_t)status);
    frf_hal_uart_puts(" listed=");
    frf_hal_uart_hex32(list.total);
    frf_hal_uart_puts("\n");
    list.total=0u;
    status=frf_fat32_list(&fs,"/roms",print_entry,&list);
    frf_hal_uart_puts("/roms directory return=");
    frf_hal_uart_hex32((uint32_t)status);
    frf_hal_uart_puts(" entries=");
    frf_hal_uart_hex32(list.total);
    frf_hal_uart_puts("\n");
    /* Seven-driver pilot lookup validates ZIP matching using FAT32. */
    status=frf_rom_catalog_scan(&fs,pilot_drivers,
        (uint32_t)(sizeof(pilot_drivers)/sizeof(pilot_drivers[0])),&pilot_index);
    frf_hal_uart_puts("Pilot ROM index status=");
    frf_hal_uart_hex32((uint32_t)status);
    frf_hal_uart_puts(" found=");
    frf_hal_uart_hex32(pilot_index.selected_count);
    frf_hal_uart_puts(" inspected=");
    frf_hal_uart_hex32(pilot_index.inspected);
    frf_hal_uart_puts("\n");
    for(unsigned i=0u;status==FRF_FAT_OK && i<pilot_index.selected_count;++i){
        frf_hal_uart_puts("ZIP candidate: ");
        frf_hal_uart_puts(pilot_drivers[pilot_index.selected[i]].name);
        frf_hal_uart_puts("\n");
    }
    frf_file_pool_init(&files,&fs);
    if(status==FRF_FAT_OK && pilot_index.selected_count){
        const char *first=pilot_drivers[pilot_index.selected[0]].name;
        char path[96]="SD:/roms/";
        unsigned len=9u;
        while(*first && len < sizeof(path)-5u)path[len++]=*first++;
        path[len++]='.';
        path[len++]='z';
        path[len++]='i';
        path[len++]='p';
        path[len]=0;
        unsigned handle=0u;
        int rc=frf_file_open(&files,path,&handle);
        frf_hal_uart_puts("Pilot read-only ROM open status=");
        frf_hal_uart_hex32((uint32_t)rc);
        if(rc==FRF_FAT_OK){
            uint32_t size=0,received=0;
            uint8_t preview[16];
            (void)frf_file_size(&files,handle,&size);
            rc=frf_file_read(&files,handle,preview,sizeof(preview),&received);
            frf_hal_uart_puts(" size=");
            frf_hal_uart_hex32(size);
            frf_hal_uart_puts(" bytes_read=");
            frf_hal_uart_hex32(received);
            frf_hal_uart_puts(" read_status=");
            frf_hal_uart_hex32((uint32_t)rc);
            (void)frf_file_close(&files,handle);
        }
        frf_hal_uart_puts("\n");
    }
    print_error("Last SDHOST state:");
    frf_hal_uart_puts("No card writes have been requested. Test ends.\n");
    for(;;) __asm__ volatile("wfe");
}
