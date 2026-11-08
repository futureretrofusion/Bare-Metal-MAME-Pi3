/*
 * Future Retro Fusion — independently implemented, read-only FAT32.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Implements published FAT32 on-disk structures, not derived from FatFs.
 */
#include "fat32.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define DIR_BYTES 32u
#define END_OF_CHAIN 0x0FFFFFF8u
#define DIR_ATTR 0x10u
#define LFN_ATTR 0x0Fu
#define FILE_ATTR 0x08u

static uint16_t rd16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int sector(frf_fat32 *fs, uint32_t lba) {
    return fs->read_sector(fs->context, lba, fs->buffer) ? FRF_FAT_OK : FRF_FAT_IO;
}
static int valid_cluster(const frf_fat32 *fs, uint32_t c) {
    return c >= 2u && c < fs->cluster_count + 2u;
}
static int ascii_upper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}
static int name_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    for (; *a && *b; ++a, ++b)
        if (ascii_upper((unsigned char)*a) != ascii_upper((unsigned char)*b)) return 0;
    return !*a && !*b;
}
static int disk_sector(frf_fat32 *fs, uint32_t cluster, uint32_t offset, uint32_t *lba) {
    if (!valid_cluster(fs, cluster) || offset >= fs->sectors_per_cluster) return FRF_FAT_CORRUPT;
    uint64_t n = (uint64_t)fs->data_lba +
                 (uint64_t)(cluster - 2u) * fs->sectors_per_cluster + offset;
    if (n >= (uint64_t)fs->volume_lba + fs->volume_sectors || n > UINT32_MAX)
        return FRF_FAT_CORRUPT;
    *lba = (uint32_t)n;
    return FRF_FAT_OK;
}
static int next_cluster(frf_fat32 *fs, uint32_t current, uint32_t *next) {
    if (!valid_cluster(fs, current)) return FRF_FAT_CORRUPT;
    uint64_t off = (uint64_t)current * 4u;
    if (off/512u >= fs->fat_sectors) return FRF_FAT_CORRUPT;
    if (sector(fs, fs->fat_lba + (uint32_t)(off/512u)) != FRF_FAT_OK) return FRF_FAT_IO;
    uint32_t c = rd32(fs->buffer + (off % 512u)) & 0x0FFFFFFFu;
    if (c >= END_OF_CHAIN) { *next = 0u; return FRF_FAT_OK; }
    if (c == 0x0FFFFFF7u || !valid_cluster(fs, c)) return FRF_FAT_CORRUPT;
    *next = c;
    return FRF_FAT_OK;
}
static int parse_volume(frf_fat32 *fs, uint32_t start, uint32_t partition_limit) {
    if (sector(fs, start)) return FRF_FAT_IO;
    const uint8_t *b = fs->buffer;
    if (b[510] != 0x55 || b[511] != 0xAA || rd16(b+11) != 512u)
        return FRF_FAT_FORMAT;
    uint32_t spc=b[13], reserved=rd16(b+14), fats=b[16];
    uint32_t total = rd16(b+19) ? rd16(b+19) : rd32(b+32);
    uint32_t fatsz = rd16(b+22) ? rd16(b+22) : rd32(b+36);
    uint32_t root = rd32(b+44);
    if (!spc || spc > 128u || (spc & (spc-1u)) || !reserved ||
        fats == 0u || fats > 4u || !fatsz || !total ||
        rd16(b+17) != 0u || rd16(b+22) != 0u || rd16(b+42) != 0u)
        return FRF_FAT_FORMAT;
    uint64_t before_data = (uint64_t)reserved + (uint64_t)fats*fatsz;
    if (before_data >= total || total > partition_limit ||
        (uint64_t)start + total > UINT32_MAX) return FRF_FAT_FORMAT;
    uint32_t clusters = (uint32_t)((total - before_data) / spc);
    /* Reject misidentified FAT12/16 and FAT32 whose FAT is too small. */
    if (clusters < 65525u || clusters > 0x0FFFFFEFu ||
        (uint64_t)(clusters+2u)*4u > (uint64_t)fatsz*512u ||
        root < 2u || root >= clusters+2u) return FRF_FAT_FORMAT;
    const uint16_t flags = rd16(b+40);
    uint32_t active = (flags & 0x80u) ? (flags & 0x0Fu) : 0u;
    if (active >= fats) return FRF_FAT_FORMAT;
    fs->volume_lba = start;
    fs->volume_sectors = total;
    fs->fat_lba = start+reserved+active*fatsz;
    fs->data_lba = start+(uint32_t)before_data;
    fs->fat_sectors = fatsz;
    fs->cluster_count = clusters;
    fs->root_cluster = root;
    fs->sectors_per_cluster = (uint8_t)spc;
    return FRF_FAT_OK;
}
int frf_fat32_mount(frf_fat32 *fs, frf_fat_read_sector reader, void *context) {
    if (!fs || !reader) return FRF_FAT_RANGE;
    memset(fs, 0, sizeof(*fs));
    fs->read_sector = reader;
    fs->context = context;
    if (sector(fs,0u)) return FRF_FAT_IO;
    if (fs->buffer[510] != 0x55u || fs->buffer[511] != 0xAAu)
        return FRF_FAT_FORMAT;
    /* Superfloppy FAT32 has a BPB at LBA zero. */
    if (rd16(fs->buffer + 11u) == 512u && fs->buffer[13] != 0u)
        return parse_volume(fs, 0u, UINT32_MAX);
    /* Find first valid primary FAT32 MBR entry, never write partition data. */
    uint32_t part = 0, count = 0;
    for (unsigned i=0; i<4u; ++i) {
        const uint8_t *entry = fs->buffer + 446u + i*16u;
        if (entry[4] == 0x0Bu || entry[4] == 0x0Cu) {
            uint32_t lba=rd32(entry+8), size=rd32(entry+12);
            if (lba && size && (uint64_t)lba + size <= UINT32_MAX) {
                part=lba; count=size; break;
            }
        }
    }
    if (!part) return FRF_FAT_FORMAT;
    return parse_volume(fs, part, count);
}
static uint8_t short_checksum(const uint8_t *s) {
    uint8_t result=0;
    for (unsigned i=0; i<11u; ++i)
        result = (uint8_t)(((result & 1u) ? 0x80u : 0u) + (result >> 1u) + s[i]);
    return result;
}
static void short_name(const uint8_t *s, char out[256]) {
    unsigned n=0;
    for (unsigned i=0; i<8u && s[i]!=' '; ++i)
        out[n++]=(char)((i==0 && s[i]==0x05u) ? 0xE5u : s[i]);
    unsigned ext=8u;
    while(ext<11u && s[ext]==' ')++ext;
    if (ext < 11u) {
        out[n++]='.';
        for (; ext<11u && s[ext]!=' '; ++ext)out[n++]=(char)s[ext];
    }
    out[n]=0;
}
typedef struct {
    uint16_t units[260];
    unsigned expected;
    uint8_t checksum;
} name_accum;
static void reset_lfn(name_accum *n) {
    n->expected=0u;
    n->checksum=0u;
}
static void lfn_piece(name_accum *n, const uint8_t *entry) {
    unsigned ordinal = entry[0] & 0x1Fu;
    if (!ordinal || ordinal > 20u || entry[12] != 0u ||
        rd16(entry+26) != 0u) { reset_lfn(n); return; }
    if (entry[0] & 0x40u) {
        for (unsigned i=0; i<260u; ++i)n->units[i]=0xFFFFu;
        n->expected=ordinal;
        n->checksum=entry[13];
    }
    if (!n->expected || ordinal != n->expected || n->checksum != entry[13]) {
        reset_lfn(n);return;
    }
    static const uint8_t offsets[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
    unsigned start=(ordinal-1u)*13u;
    for (unsigned j=0;j<13u;++j)n->units[start+j]=rd16(entry+offsets[j]);
    --n->expected;
}
static int long_name(const name_accum *n, const uint8_t *short_entry, char out[256]) {
    if (n->expected || n->checksum != short_checksum(short_entry))
        return 0;
    unsigned count=0;
    for (unsigned i=0; i<260u; ++i) {
        uint16_t c=n->units[i];
        if (c == 0u || c == 0xFFFFu)break;
        if (c < 0x20u || c > 0x7Eu || count>=255u) return 0;
        out[count++]=(char)c;
    }
    if (!count)return 0;
    out[count]=0;
    return 1;
}
/* Return 1 only when caller requests early successful stop.
 * Return negative for I/O, invalid directory chains or corrupt sectors. */
static int scan_directory(frf_fat32 *fs, uint32_t first,
                          frf_fat32_list_callback callback, void *context) {
    if (!valid_cluster(fs, first)) return FRF_FAT_CORRUPT;
    uint32_t cluster=first;
    name_accum lfn={0};
    frf_fat32_entry item;
    for (uint32_t step=0; step<fs->cluster_count; ++step) {
        for (uint32_t j=0; j<fs->sectors_per_cluster; ++j) {
            uint32_t lba;
            if (disk_sector(fs,cluster,j,&lba)) return FRF_FAT_CORRUPT;
            if (sector(fs,lba)) return FRF_FAT_IO;
            for (unsigned p=0; p<512u; p+=DIR_BYTES) {
                const uint8_t *e=fs->buffer+p;
                if (e[0]==0u) return FRF_FAT_OK;
                if (e[0]==0xE5u) { reset_lfn(&lfn); continue; }
                if ((e[11]&0x3Fu)==LFN_ATTR) {
                    lfn_piece(&lfn,e);
                    continue;
                }
                if (e[11]&FILE_ATTR) { reset_lfn(&lfn);continue; }
                if (!long_name(&lfn,e,item.name))short_name(e,item.name);
                reset_lfn(&lfn);
                item.attributes=e[11];
                item.first_cluster=((uint32_t)rd16(e+20)<<16) | rd16(e+26);
                item.size=rd32(e+28);
                if (callback(&item,context))return 1;
            }
        }
        uint32_t next=0;
        int status=next_cluster(fs,cluster,&next);
        if(status)return status;
        if(!next)return FRF_FAT_OK;
        cluster=next;
    }
    return FRF_FAT_CORRUPT;
}
typedef struct {
    const char *wanted;
    frf_fat32_entry entry;
    int found;
} search_request;
static int match_cb(const frf_fat32_entry *item,void *context) {
    search_request *r=(search_request *)context;
    if(!name_equal(item->name,r->wanted))return 0;
    r->entry=*item;
    r->found=1;
    return 1;
}
static int find(frf_fat32 *fs, const char *path, frf_fat32_entry *out,
                int require_dir) {
    if (!fs||!path||!out) return FRF_FAT_RANGE;
    uint32_t parent=fs->root_cluster;
    const char *p=path;
    while (*p=='/'||*p=='\\')++p;
    if (!*p) {
        memset(out,0,sizeof(*out));out->first_cluster=parent;
        out->attributes=DIR_ATTR;
        return FRF_FAT_OK;
    }
    char component[256];
    for(;;) {
        unsigned n=0;
        while(*p && *p!='/' && *p!='\\') {
            if(n>=255u)return FRF_FAT_RANGE;
            component[n++]=*p++;
        }
        if(!n)return FRF_FAT_RANGE;
        component[n]=0;
        while(*p=='/'||*p=='\\')++p;
        search_request s={component,{ {0},0u,0u,0u },0};
        int status=scan_directory(fs,parent,match_cb,&s);
        if(status<0)return status;
        if(!s.found)return FRF_FAT_NOT_FOUND;
        if(*p || require_dir) {
            if(!(s.entry.attributes&DIR_ATTR))return FRF_FAT_NOT_FILE;
        }
        if(!*p) {
            *out=s.entry;
            return FRF_FAT_OK;
        }
        if(!valid_cluster(fs,s.entry.first_cluster))return FRF_FAT_CORRUPT;
        parent=s.entry.first_cluster;
    }
}
int frf_fat32_list(frf_fat32 *fs, const char *directory,
                    frf_fat32_list_callback callback,void *context) {
    if (!fs || !callback) return FRF_FAT_RANGE;
    frf_fat32_entry d;
    int s=find(fs,directory?directory:"/",&d,1);
    if(s)return s;
    s=scan_directory(fs,d.first_cluster,callback,context);
    return s==1 ? FRF_FAT_OK : s;
}
int frf_fat32_open(frf_fat32 *fs, const char *path, frf_fat32_file *file) {
    if(!fs||!path||!file)return FRF_FAT_RANGE;
    frf_fat32_entry e;
    int s=find(fs,path,&e,0);
    if(s)return s;
    if(e.attributes & DIR_ATTR)return FRF_FAT_NOT_FILE;
    if(e.size && !valid_cluster(fs,e.first_cluster))return FRF_FAT_CORRUPT;
    file->first_cluster=e.first_cluster;
    file->size=e.size;
    file->position=0u;
    file->current_cluster=e.first_cluster;
    file->current_cluster_index=0u;
    return FRF_FAT_OK;
}
int frf_fat32_seek(frf_fat32_file *file, uint32_t offset) {
    if(!file || offset>file->size)return FRF_FAT_RANGE;
    file->position=offset;
    if(offset==0u) {
        file->current_cluster=file->first_cluster;
        file->current_cluster_index=0u;
    }
    return FRF_FAT_OK;
}
int frf_fat32_read(frf_fat32 *fs, frf_fat32_file *file,
                   void *output, uint32_t requested, uint32_t *actual) {
    if(!fs||!file||!output||!actual)return FRF_FAT_RANGE;
    *actual=0u;
    if(!requested || file->position>=file->size)return FRF_FAT_OK;
    uint8_t *dest=(uint8_t *)output;
    uint32_t bytes_per_cluster=(uint32_t)fs->sectors_per_cluster*512u;
    if(!bytes_per_cluster || !valid_cluster(fs,file->first_cluster))
        return FRF_FAT_CORRUPT;
    uint32_t remaining=file->size-file->position;
    if(requested<remaining)remaining=requested;
    while(remaining) {
        uint32_t index=file->position / bytes_per_cluster;
        if(index<file->current_cluster_index) {
            file->current_cluster=file->first_cluster;
            file->current_cluster_index=0u;
        }
        while(file->current_cluster_index<index) {
            uint32_t next=0;
            int status=next_cluster(fs,file->current_cluster,&next);
            if(status)return status;
            if(!next)return FRF_FAT_CORRUPT;
            file->current_cluster=next;
            ++file->current_cluster_index;
        }
        uint32_t inside=file->position % bytes_per_cluster;
        uint32_t lba;
        if(disk_sector(fs,file->current_cluster,inside/512u,&lba))
            return FRF_FAT_CORRUPT;
        int status=sector(fs,lba);
        if(status)return status;
        uint32_t start=inside%512u;
        uint32_t amount=512u-start;
        if(amount>remaining)amount=remaining;
        memcpy(dest+*actual,fs->buffer+start,amount);
        file->position+=amount;
        *actual+=amount;
        remaining-=amount;
    }
    return FRF_FAT_OK;
}
