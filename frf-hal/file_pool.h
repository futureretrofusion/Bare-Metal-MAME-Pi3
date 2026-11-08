/*
 * Future Retro Fusion — ROM file descriptor compatibility layer.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * MAME-facing read-only file operations over the independent FAT32 reader.
 * No Circle, newlib/FILE*, heap, card write, Linux or SD driver dependency.
 */
#ifndef FRF_HAL_FILE_POOL_H
#define FRF_HAL_FILE_POOL_H
#include "fat32.h"
#include <stdint.h>
enum { FRF_FILE_MAX_OPEN = 32 };
enum { FRF_SEEK_START = 0, FRF_SEEK_CURRENT = 1, FRF_SEEK_END = 2 };
typedef struct {
    frf_fat32 *volume;
    frf_fat32_file files[FRF_FILE_MAX_OPEN];
    uint32_t active_bits;
} frf_file_pool;
void frf_file_pool_init(frf_file_pool *pool, frf_fat32 *volume);
int frf_file_open(frf_file_pool *pool, const char *path, unsigned *handle);
int frf_file_read(frf_file_pool *pool, unsigned handle, void *buffer,
                  uint32_t length, uint32_t *received);
int frf_file_seek(frf_file_pool *pool, unsigned handle,
                  int64_t offset, int whence);
int frf_file_tell(const frf_file_pool *pool, unsigned handle,
                  uint32_t *position);
int frf_file_size(const frf_file_pool *pool, unsigned handle, uint32_t *size);
int frf_file_close(frf_file_pool *pool, unsigned handle);
#endif
