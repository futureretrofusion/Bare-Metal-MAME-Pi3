/*
 * Future Retro Fusion — independent read-only FAT32 reader.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No FatFs, Circle, operating system, heap, or copyrighted ROM dependency.
 *
 * Design: one 512-byte scratch sector owned by a filesystem instance.
 * Not thread-safe: serialize calls to the same filesystem instance.
 * Supported: MBR type 0x0B/0x0C, FAT32 "superfloppy", 8.3 and ASCII VFAT
 * names, subdirectories, bounded FAT chains, random-access regular files.
 * FAT12/16, exFAT, Unicode names and write access are NOT implemented.
 */
#ifndef FRF_HAL_FAT32_H
#define FRF_HAL_FAT32_H
#include <stddef.h>
#include <stdint.h>
enum {
    FRF_FAT_OK = 0,
    FRF_FAT_IO = -1,
    FRF_FAT_FORMAT = -2,
    FRF_FAT_NOT_FOUND = -3,
    FRF_FAT_CORRUPT = -4,
    FRF_FAT_RANGE = -5,
    FRF_FAT_NOT_FILE = -6
};
typedef int (*frf_fat_read_sector)(void *context, uint32_t lba, uint8_t sector[512]);
typedef struct {
    frf_fat_read_sector read_sector;
    void *context;
    uint32_t volume_lba;
    uint32_t volume_sectors;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t fat_sectors;
    uint32_t cluster_count;
    uint32_t root_cluster;
    uint8_t sectors_per_cluster;
    uint8_t buffer[512];
} frf_fat32;
typedef struct {
    char name[256];
    uint32_t first_cluster;
    uint32_t size;
    uint8_t attributes;
} frf_fat32_entry;
typedef int (*frf_fat32_list_callback)(const frf_fat32_entry *entry, void *context);
typedef struct {
    uint32_t first_cluster;
    uint32_t size;
    uint32_t position;
    uint32_t current_cluster;
    uint32_t current_cluster_index;
} frf_fat32_file;
int frf_fat32_mount(frf_fat32 *fs, frf_fat_read_sector reader, void *context);
int frf_fat32_list(frf_fat32 *fs, const char *directory,
                    frf_fat32_list_callback callback, void *context);
int frf_fat32_open(frf_fat32 *fs, const char *path, frf_fat32_file *file);
int frf_fat32_seek(frf_fat32_file *file, uint32_t offset);
int frf_fat32_read(frf_fat32 *fs, frf_fat32_file *file,
                    void *output, uint32_t requested, uint32_t *actual);
#endif
