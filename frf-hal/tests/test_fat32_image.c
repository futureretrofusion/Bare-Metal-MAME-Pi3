/*
 * FRF independent FAT32 — validate a real mkfs.fat image (READ-ONLY).
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Usage: host-test-fat32-image /absolute/path/to/scratch-fat32.img
 * This program never opens its argument for writing.
 */
#include "../fat32.h"
#include "../rom_catalog.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
static FILE *disk;
static int read_sector(void *ctx,uint32_t lba,uint8_t out[512]){
    FILE *fp=(FILE *)ctx;
    if(fseek(fp,(long)((uint64_t)lba*512u),SEEK_SET)!=0)return 0;
    return fread(out,1,512u,fp)==512u;
}
typedef struct {unsigned files;unsigned folders;unsigned zipfiles;} directory_info;
static int observe(const frf_fat32_entry *e,void *ctx){
    directory_info *dir=(directory_info *)ctx;
    if(e->attributes & 0x10u)++dir->folders;
    else ++dir->files;
    size_t n=strlen(e->name);
    if(n>4u && !strcmp(e->name+n-4u,".ZIP"))++dir->zipfiles;
    return 0;
}
static const frf_rom_driver choices[]={
    {"outRun","Out Run"},
    {"galaga","Galaga"},
    {"sf2","Street Fighter II"},
    {"pacman","Pac-Man"},
    {"neogeo","BIOS"}
};
static frf_rom_catalog catalog;
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: %s test-fat32.img\n",argv[0]);return 2;}
    disk=fopen(argv[1],"rb");
    if(!disk){perror("fopen");return 2;}
    frf_fat32 fs;
    int rc=frf_fat32_mount(&fs,read_sector,disk);
    if(rc){fprintf(stderr,"FAT32 mount failed: %d\n",rc);fclose(disk);return 1;}
    directory_info info={0};
    rc=frf_fat32_list(&fs,"/roms",observe,&info);
    if(rc){fprintf(stderr,"ROM directory failed: %d\n",rc);fclose(disk);return 1;}
    if(info.files<3u){fprintf(stderr,"missing sample ZIP files: %u\n",info.files);fclose(disk);return 1;}
    rc=frf_rom_catalog_scan(&fs,choices,5u,&catalog);
    if(rc || catalog.selected_count!=3u){
        fprintf(stderr,"ROM catalog mismatch rc=%d matched=%u\n",rc,(unsigned)catalog.selected_count);
        fclose(disk);return 1;
    }
    if(catalog.selected[0]!=1u || catalog.selected[1]!=0u || catalog.selected[2]!=2u){
        fprintf(stderr,"game description sorting incorrect\n");fclose(disk);return 1;
    }
    frf_fat32_file f;
    rc=frf_fat32_open(&fs,"/roms/GALAGA.ZIP",&f);
    if(rc){fprintf(stderr,"GALAGA open failed: %d\n",rc);fclose(disk);return 1;}
    char out[256];
    uint32_t got=0;
    rc=frf_fat32_read(&fs,&f,out,sizeof(out),&got);
    if(rc || got<15u || memcmp(out,"FRF-GALAGA-FIXTURE",18u)){
        fprintf(stderr,"GALAGA read failed rc=%d got=%u\n",rc,(unsigned)got);
        fclose(disk);return 1;
    }
    rc=frf_fat32_open(&fs,"/roms/OutRun Arcade.zip",&f);
    if(rc){fprintf(stderr,"LFN open failed: %d\n",rc);fclose(disk);return 1;}
    rc=frf_fat32_read(&fs,&f,out,sizeof(out),&got);
    if(rc|| got<18u || memcmp(out,"FRF-OUTRUN-FIXTURE",18u)){
        fprintf(stderr,"LFN contents failed: %d\n",rc);fclose(disk);return 1;
    }
    printf("FRF real FAT32 image: mounted, files=%u, matched ROMs=%u, LFN and data PASS\n",
            info.files,(unsigned)catalog.selected_count);
    fclose(disk);return 0;
}
