/*
 * FRF ROM catalog, independent directory scanning and driver matching.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Performance: O(driver count * log driver count + ZIP count * log drivers).
 * Uses existing MAME short names and display titles; no realloc/qsort.
 * No ZIP content parsed. Do not confuse archive presence with valid ROMs.
 */
#include "rom_catalog.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static int cmpascii(char a,char b) {
    unsigned x=(unsigned char)a,y=(unsigned char)b;
    if(x>='A' && x<='Z')x+=32u;
    if(y>='A' && y<='Z')y+=32u;
    return (int)x-(int)y;
}
static int compare(const char *a,const char *b) {
    if(!a)a="";
    if(!b)b="";
    for(;*a && *b;a++,b++) {
        int d=cmpascii(*a,*b);
        if(d)return d;
    }
    return cmpascii(*a,*b);
}
static int cmp_name(const frf_rom_driver *drivers,uint16_t a,uint16_t b) {
    return compare(drivers[a].name,drivers[b].name);
}
static int cmp_title(const frf_rom_driver *drivers,uint16_t a,uint16_t b) {
    int d=compare(drivers[a].description,drivers[b].description);
    return d ? d : cmp_name(drivers,a,b);
}
static void sift(uint16_t *items,unsigned root,unsigned end,
                 const frf_rom_driver *drivers,int title){
    for(;;) {
        unsigned child=root*2u+1u;
        if(child>end)return;
        if(child+1u<=end &&
           (title?cmp_title(drivers,items[child],items[child+1u]):
                  cmp_name(drivers,items[child],items[child+1u]))<0)child++;
        if((title?cmp_title(drivers,items[root],items[child]):
                  cmp_name(drivers,items[root],items[child]))>=0)return;
        uint16_t tmp=items[root];items[root]=items[child];items[child]=tmp;
        root=child;
    }
}
static void heap_sort(uint16_t *a,unsigned n,
                      const frf_rom_driver *d,int title) {
    if(n<2u)return;
    for(unsigned i=n/2u;i>0u;) {
        --i;sift(a,i,n-1u,d,title);
    }
    for(unsigned end=n-1u;end>0u;--end) {
        uint16_t tmp=a[0];a[0]=a[end];a[end]=tmp;
        sift(a,0u,end-1u,d,title);
    }
}
typedef struct {
    const frf_rom_driver *drivers;
    frf_rom_catalog *catalog;
    uint32_t count;
    int overflow;
} scan_context;
static int visit(const frf_fat32_entry *e,void *user) {
    scan_context *ctx=(scan_context *)user;
    frf_rom_catalog *out=ctx->catalog;
    ++out->inspected;
    if(e->attributes&0x10u)return 0; /* ignore subdirectories */
    size_t n=0u;
    while(n<sizeof(e->name) && e->name[n])++n;
    if(n<=4u || n>=sizeof(e->name) || e->name[n-4u]!='.' ||
       cmpascii(e->name[n-3u],'z') ||
       cmpascii(e->name[n-2u],'i') ||
       cmpascii(e->name[n-1u],'p'))return 0;
    ++out->zip_count;
    char base[256];
    for(size_t j=0u;j<n-4u;++j)base[j]=e->name[j];
    base[n-4u]=0;
    if(!compare(base,"neogeo"))return 0; /* shared BIOS, not a game */
    unsigned lo=0u,hi=ctx->count;
    while(lo<hi) {
        unsigned mid=lo+(hi-lo)/2u;
        unsigned index=out->by_shortname[mid];
        int cmp=compare(ctx->drivers[index].name,base);
        if(cmp<0)lo=mid+1u;else hi=mid;
    }
    if(lo>=ctx->count)return 0;
    uint16_t game=out->by_shortname[lo];
    if(compare(ctx->drivers[game].name,base))return 0;
    const uint16_t flag=(uint16_t)(1u<<(game&7u));
    if(out->seen[game>>3u] & flag)return 0; /* duplicate archive alias */
    if(out->selected_count>=FRF_ROM_MAX_DRIVERS) {
        ctx->overflow=1;
        return 1;
    }
    out->seen[game>>3u] |= (uint8_t)flag;
    out->selected[out->selected_count++]=game;
    return 0;
}
int frf_rom_catalog_scan(frf_fat32 *fs,const frf_rom_driver *drivers,
                         uint32_t count,frf_rom_catalog *out) {
    if(!fs||!drivers||!out || count>FRF_ROM_MAX_DRIVERS) return FRF_FAT_RANGE;
    memset(out,0,sizeof(*out));
    for(uint32_t i=0;i<count;i++) {
        if(!drivers[i].name || !drivers[i].description)
            return FRF_FAT_RANGE;
        out->by_shortname[i]=(uint16_t)i;
    }
    out->driver_count=(uint16_t)count;
    heap_sort(out->by_shortname,count,drivers,0);
    scan_context ctx={drivers,out,count,0};
    int rc=frf_fat32_list(fs,"/roms",visit,&ctx);
    if(rc)return rc;
    if(ctx.overflow)return FRF_FAT_RANGE;
    heap_sort(out->selected,out->selected_count,drivers,1);
    return FRF_FAT_OK;
}
