/*
 * FRF HAL FAT32 read-only parser synthetic disk tests (no SD hardware).
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../fat32.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define START 1u
#define RESERVED 32u
#define FATS 513u
#define CLUSTERS 65526u
#define TOTAL (RESERVED+2u*FATS+CLUSTERS)
#define DATA (START+RESERVED+2u*FATS)
static int bad_fat=0, corrupt_name=0, io_failure=0;
static void w16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void w32(uint8_t *p,uint32_t x){w16(p,(uint16_t)x);w16(p+2,(uint16_t)(x>>16));}
static uint8_t checksum(const char s[11]){
    uint8_t c=0;
    for(unsigned i=0;i<11;++i)c=(uint8_t)(((c&1u)?0x80u:0u)+(c>>1)+(uint8_t)s[i]);
    return c;
}
static void entry(uint8_t *p,const char sn[11],uint8_t attr,
                  uint32_t cluster,uint32_t size){
    memset(p,0,32);
    memcpy(p,sn,11);
    p[11]=attr;
    w16(p+20,(uint16_t)(cluster>>16));
    w16(p+26,(uint16_t)cluster);
    w32(p+28,size);
}
static void lfn(uint8_t *p,const char *value,unsigned slot,unsigned total,uint8_t sum){
    const unsigned positions[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
    memset(p,0xff,32);
    p[0]=(uint8_t)(slot | (slot==total?0x40u:0u));
    p[11]=0x0f;
    p[12]=0;
    p[13]=sum;
    w16(p+26,0);
    size_t len=strlen(value);
    for(unsigned i=0;i<13u;++i){
        unsigned at=(slot-1u)*13u+i;
        uint16_t c=at<len?(uint8_t)value[at]:at==len?0u:0xffffu;
        w16(p+positions[i],c);
    }
}
static int read_disk(void *ctx,uint32_t lba,uint8_t out[512]){
    (void)ctx;
    if (io_failure && lba == DATA+1u)return 0;
    memset(out,0,512);
    if(lba==0){
        out[510]=0x55;out[511]=0xaa;
        uint8_t *p=out+446;
        p[4]=0x0c;
        w32(p+8,START);
        w32(p+12,TOTAL);
    } else if(lba==START){
        out[0]=0xeb;out[1]=0x58;out[2]=0x90;
        memcpy(out+3,"FRFTEST ",8);
        w16(out+11,512);
        out[13]=1;w16(out+14,RESERVED);out[16]=2;
        w32(out+32,TOTAL);w32(out+36,FATS);
        w32(out+44,2);w16(out+48,1);w16(out+50,6);
        out[66]=0x29;memcpy(out+82,"FAT32   ",8);
        out[510]=0x55;out[511]=0xaa;
    } else if(lba==START+RESERVED || lba==START+RESERVED+FATS){
        uint32_t *unused=0;(void)unused;
        w32(out+0,0x0ffffff8);
        w32(out+4,0x0fffffff);
        w32(out+8,0x0fffffff); /* root cluster2 */
        w32(out+12,bad_fat?0x0fffffff:4u);
        w32(out+16,0x0fffffff);
        w32(out+20,0x0fffffff);
        w32(out+24,0x0fffffff);
        w32(out+28,0x0fffffff);
    } else if(lba==DATA){
        static const char sn[]="OUTRUN~1ZIP";
        entry(out,"HELLO   TXT",0x20,3u,700u);
        uint8_t sum=checksum(sn);
        lfn(out+32,"OutRun Arcade.zip",2u,2u,corrupt_name?(uint8_t)(sum+1u):sum);
        lfn(out+64,"OutRun Arcade.zip",1u,2u,corrupt_name?(uint8_t)(sum+1u):sum);
        entry(out+96,sn,0x20u,5u,50u);
        entry(out+128,"ROMS       ",0x10u,6u,0u);
    } else if(lba==DATA+1u){
        memset(out,'A',512);
    } else if(lba==DATA+2u){
        memset(out,'B',512);
    } else if(lba==DATA+3u){
        memset(out,'Z',512);
    } else if(lba==DATA+4u){
        entry(out,"GALAGA  ZIP",0x20u,7u,24u);
    } else if(lba==DATA+5u){
        memset(out,'G',512);
    }
    return 1;
}
typedef struct {unsigned count;int has_long;int has_dir;} listing;
static int listed(const frf_fat32_entry *e,void *ctx){
    listing *l=(listing *)ctx;
    l->count++;
    if(!strcmp(e->name,"OutRun Arcade.zip"))l->has_long=1;
    if(!strcmp(e->name,"ROMS"))l->has_dir=1;
    return 0;
}
int main(void){
    frf_fat32 fs;
    frf_fat32_file f;
    uint32_t actual=0u;
    char result[800];
    assert(frf_fat32_mount(&fs,read_disk,0)==FRF_FAT_OK);
    assert(fs.volume_lba==START && fs.sectors_per_cluster==1);
    assert(fs.data_lba==DATA && fs.cluster_count==CLUSTERS);
    listing names={0};
    assert(frf_fat32_list(&fs,"/",listed,&names)==FRF_FAT_OK);
    assert(names.count==3u && names.has_long && names.has_dir);
    assert(frf_fat32_open(&fs,"/hello.txt",&f)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,700u,&actual)==FRF_FAT_OK);
    assert(actual==700u && f.position==700u);
    for(unsigned i=0;i<512u;++i)assert(result[i]=='A');
    for(unsigned i=512;i<700u;++i)assert(result[i]=='B');
    assert(frf_fat32_seek(&f,510u)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,5u,&actual)==FRF_FAT_OK);
    assert(actual==5u && result[0]=='A' && result[1]=='A' && result[2]=='B');
    assert(frf_fat32_seek(&f,701u)==FRF_FAT_RANGE);
    assert(frf_fat32_open(&fs,"/outrun arcade.zip",&f)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,512u,&actual)==FRF_FAT_OK);
    assert(actual==50u && result[49]=='Z');
    assert(frf_fat32_open(&fs,"/ROMs/galaga.zip",&f)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,24u,&actual)==FRF_FAT_OK);
    assert(actual==24u && result[23]=='G');
    assert(frf_fat32_open(&fs,"/ROMs",&f)==FRF_FAT_NOT_FILE);
    assert(frf_fat32_open(&fs,"/missing.zip",&f)==FRF_FAT_NOT_FOUND);
    corrupt_name=1;
    assert(frf_fat32_open(&fs,"/OutRun Arcade.zip",&f)==FRF_FAT_NOT_FOUND);
    assert(frf_fat32_open(&fs,"/OUTRUN~1.ZIP",&f)==FRF_FAT_OK);
    corrupt_name=0;
    bad_fat=1;
    assert(frf_fat32_open(&fs,"/HELLO.TXT",&f)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,700u,&actual)==FRF_FAT_CORRUPT);
    assert(actual==512u);
    bad_fat=0;
    io_failure=1;
    assert(frf_fat32_open(&fs,"/HELLO.TXT",&f)==FRF_FAT_OK);
    assert(frf_fat32_read(&fs,&f,result,100u,&actual)==FRF_FAT_IO);
    assert(actual==0u);
    puts("FRF independent FAT32 MBR, LFN, directories, seek, chains and errors: PASS");
    return 0;
}
