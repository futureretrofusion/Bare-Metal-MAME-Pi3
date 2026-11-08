/*
 * Future Retro Fusion — fixed-size read-only MAME ROM file adapter.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * One filesystem sector cache shared by all handles; non-reentrant.
 */
#include "file_pool.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static int valid(const frf_file_pool *p,unsigned h) {
    return p && p->volume && h>=1u && h<=FRF_FILE_MAX_OPEN &&
           (p->active_bits & (1u<<(h-1u)))!=0u;
}
void frf_file_pool_init(frf_file_pool *pool,frf_fat32 *volume) {
    if(!pool)return;
    memset(pool,0,sizeof(*pool));
    pool->volume=volume;
}
static const char *translate_path(const char *path) {
    if(!path)return 0;
    if((path[0]=='S'||path[0]=='s') &&
       (path[1]=='D'||path[1]=='d') && path[2]==':')return path+3u;
    return path;
}
int frf_file_open(frf_file_pool *p,const char *path,unsigned *h) {
    if(!p || !p->volume || !path || !h)return FRF_FAT_RANGE;
    *h=0u;
    unsigned slot=0u;
    while(slot<FRF_FILE_MAX_OPEN && (p->active_bits&(1u<<slot)))++slot;
    if(slot==FRF_FILE_MAX_OPEN)return FRF_FAT_RANGE;
    int rc=frf_fat32_open(p->volume,translate_path(path),&p->files[slot]);
    if(rc)return rc;
    p->active_bits|=(1u<<slot);
    *h=slot+1u;
    return FRF_FAT_OK;
}
int frf_file_read(frf_file_pool *p,unsigned h,void *buffer,
                  uint32_t n,uint32_t *received) {
    if(!valid(p,h)||!received||(!buffer && n))return FRF_FAT_RANGE;
    if(!n){*received=0u;return FRF_FAT_OK;}
    return frf_fat32_read(p->volume,&p->files[h-1u],buffer,n,received);
}
int frf_file_seek(frf_file_pool *p,unsigned h,int64_t off,int whence) {
    if(!valid(p,h))return FRF_FAT_RANGE;
    frf_fat32_file *f=&p->files[h-1u];
    uint32_t base=0;
    if(whence==FRF_SEEK_CURRENT)base=f->position;
    else if(whence==FRF_SEEK_END)base=f->size;
    else if(whence!=FRF_SEEK_START)return FRF_FAT_RANGE;
    if(off<-(int64_t)base ||
       off>(int64_t)f->size-(int64_t)base)return FRF_FAT_RANGE;
    return frf_fat32_seek(f,(uint32_t)((int64_t)base+off));
}
int frf_file_tell(const frf_file_pool *p,unsigned h,uint32_t *position) {
    if(!valid(p,h)||!position)return FRF_FAT_RANGE;
    *position=p->files[h-1u].position;return FRF_FAT_OK;
}
int frf_file_size(const frf_file_pool *p,unsigned h,uint32_t *size) {
    if(!valid(p,h)||!size)return FRF_FAT_RANGE;
    *size=p->files[h-1u].size;return FRF_FAT_OK;
}
int frf_file_close(frf_file_pool *p,unsigned h) {
    if(!valid(p,h))return FRF_FAT_RANGE;
    p->active_bits&=~(1u<<(h-1u));
    memset(&p->files[h-1u],0,sizeof(p->files[0]));
    return FRF_FAT_OK;
}
