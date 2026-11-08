/*
 * FRF HAL independently reconstructed ROM-index logic host unit test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Simulates the output of the verified FAT32 directory parser.
 */
#include "../rom_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const frf_rom_driver drivers[] = {
    {"sf2","Street Fighter II"},
    {"pacman","Pac-Man"},
    {"outrun","Out Run"},
    {"neogeo","BIOS only"},
    {"galaga","Galaga"},
    {"commando","Commando"}
};
static void supply(frf_fat32_list_callback fn,void *ctx,const char *name,uint8_t attrs) {
    frf_fat32_entry e={0};
    memcpy(e.name,name,strlen(name)+1u);
    e.attributes=attrs;
    e.size=300u;
    fn(&e,ctx);
}
int frf_fat32_list(frf_fat32 *fs, const char *path,
                    frf_fat32_list_callback callback,void *ctx) {
    (void)fs;
    if(strcmp(path,"/roms"))return FRF_FAT_NOT_FOUND;
    supply(callback,ctx,"Sf2.ZIP",0x20u);
    supply(callback,ctx,"GALAGA.zip",0x20u);
    supply(callback,ctx,"NEOGEO.ZIP",0x20u);
    supply(callback,ctx,"sf2.zip",0x20u); /* duplicate */
    supply(callback,ctx,"outRun.zip",0x20u);
    supply(callback,ctx,"PACMAN.ZIP",0x20u);
    supply(callback,ctx,"commando.7z",0x20u); /* unsupported extension */
    supply(callback,ctx,"garbage.zip",0x20u);
    supply(callback,ctx,"commando.zip",0x10u); /* directory, skip */
    return FRF_FAT_OK;
}
static frf_rom_catalog c;
int main(void) {
    frf_fat32 fs={0};
    int r=frf_rom_catalog_scan(&fs,drivers,6u,&c);
    assert(r==FRF_FAT_OK && c.driver_count==6u);
    assert(c.inspected==9u && c.zip_count==7u);
    assert(c.selected_count==4u);
    assert(c.selected[0]==4u); /* Galaga */
    assert(c.selected[1]==2u); /* Out Run */
    assert(c.selected[2]==1u); /* Pac-Man */
    assert(c.selected[3]==0u); /* Street Fighter II */
    assert(frf_rom_catalog_scan(&fs,drivers,8193u,&c)==FRF_FAT_RANGE);
    assert(frf_rom_catalog_scan(0,drivers,6u,&c)==FRF_FAT_RANGE);
    puts("FRF ROM ZIP index, filtering, deduplication, sorting: PASS");
    return 0;
}
