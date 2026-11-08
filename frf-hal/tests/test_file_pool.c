/*
 * FRF HAL fixed-pool file handle integration test against a scratch FAT32 image.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../fat32.h"
#include "../file_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static int reader(void *ctx,uint32_t lba,uint8_t b[512]){
    FILE *f=(FILE*)ctx;
    if(fseek(f,(long)((uint64_t)lba*512u),SEEK_SET))return 0;
    return fread(b,1,512u,f)==512u;
}
#define MUST(condition) do { if(!(condition)) { \
  fprintf(stderr,"file_pool test failure at line %d\n",__LINE__); \
  return 1; }} while(0)
static frf_file_pool pool;
int main(int argc,char **argv){
    MUST(argc==2);
    FILE *disk=fopen(argv[1],"rb");
    MUST(disk!=0);
    frf_fat32 fs;
    MUST(frf_fat32_mount(&fs,reader,disk)==FRF_FAT_OK);
    frf_file_pool_init(&pool,&fs);
    unsigned h1=0,h2=0;
    MUST(frf_file_open(&pool,"SD:/roms/GALAGA.zip",&h1)==FRF_FAT_OK);
    MUST(h1>0u);
    MUST(frf_file_open(&pool,"/roms/OutRun Arcade.zip",&h2)==FRF_FAT_OK);
    MUST(h1!=h2);
    char data[64]={0};
    uint32_t got=0u,size=0u,position=0u;
    MUST(frf_file_read(&pool,h1,data,3u,&got)==FRF_FAT_OK);
    MUST(got==3u && !memcmp(data,"FRF",3u));
    MUST(frf_file_read(&pool,h2,data,18u,&got)==FRF_FAT_OK);
    MUST(got==18u && !memcmp(data,"FRF-OUTRUN-FIXTURE",18u));
    MUST(frf_file_tell(&pool,h1,&position)==FRF_FAT_OK && position==3u);
    MUST(frf_file_size(&pool,h2,&size)==FRF_FAT_OK && size>=18u);
    MUST(frf_file_seek(&pool,h1,0,FRF_SEEK_END)==FRF_FAT_OK);
    MUST(frf_file_read(&pool,h1,data,2u,&got)==FRF_FAT_OK && got==0u);
    MUST(frf_file_seek(&pool,h1,-4,FRF_SEEK_END)==FRF_FAT_OK);
    MUST(frf_file_read(&pool,h1,data,4u,&got)==FRF_FAT_OK && got==4u);
    MUST(frf_file_seek(&pool,h1,1,FRF_SEEK_END)==FRF_FAT_RANGE);
    MUST(frf_file_seek(&pool,h1,-1,FRF_SEEK_START)==FRF_FAT_RANGE);
    MUST(frf_file_seek(&pool,h1,0,99)==FRF_FAT_RANGE);
    MUST(frf_file_close(&pool,h1)==FRF_FAT_OK);
    MUST(frf_file_read(&pool,h1,data,4u,&got)==FRF_FAT_RANGE);
    MUST(frf_file_close(&pool,h2)==FRF_FAT_OK);
    unsigned handles[FRF_FILE_MAX_OPEN];
    for(unsigned i=0;i<FRF_FILE_MAX_OPEN;++i)
        MUST(frf_file_open(&pool,"/roms/SF2.zip",&handles[i])==FRF_FAT_OK);
    MUST(frf_file_open(&pool,"/roms/SF2.zip",&h1)==FRF_FAT_RANGE);
    for(unsigned i=0;i<FRF_FILE_MAX_OPEN;++i)
        MUST(frf_file_close(&pool,handles[i])==FRF_FAT_OK);
    MUST(pool.active_bits==0u);
    fclose(disk);
    puts("FRF ROM file handles: SD path, interleaved read, SEEK, EOF, pool exhaustion PASS");
    return 0;
}
