/*
 * Future Retro Fusion / BCM2837 independent SDHOST block-read interface.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Targets Pi 3 SDHOST 0x3F202000, NOT Arasan 0x3F300000.
 * The boot ROM/firmware and board pin mux must route the SD socket to
 * SDHOST (GPIO48..53). No writes to card, no DMA and no GPL code.
 */
#ifndef FRF_HAL_SDHOST_H
#define FRF_HAL_SDHOST_H
#include <stdint.h>
#define FRF_SDHOST_BASE 0x3F202000u
#define FRF_SDHOST_CMD_START 0x8000u
#define FRF_SDHOST_CMD_NORESP 0x0400u
#define FRF_SDHOST_CMD_LONGRESP 0x0200u
#define FRF_SDHOST_CMD_READ 0x0040u
static inline uint32_t frf_sdhost_cmd_word(unsigned command, int no_reply,
                                           int long_reply, int read_data) {
    return FRF_SDHOST_CMD_START | ((uint32_t)command & 63u) |
           (no_reply ? FRF_SDHOST_CMD_NORESP : 0u) |
           (long_reply ? FRF_SDHOST_CMD_LONGRESP : 0u) |
           (read_data ? FRF_SDHOST_CMD_READ : 0u);
}
static inline int frf_sdhost_card_arg(uint32_t lba, int high_capacity,
                                      uint32_t *value) {
    if (!value || (!high_capacity && lba > UINT32_MAX/512u))return 0;
    *value=high_capacity ? lba : lba*512u;
    return 1;
}
typedef struct {
    uint32_t rca;
    uint32_t ocr;
    uint32_t last_command;
    uint32_t last_response;
    uint32_t last_status;
    uint32_t last_edm;
    uint32_t last_command_word;
    uint32_t sectors_read;
    uint8_t high_capacity;
    uint8_t initialized;
} frf_sdhost;
void frf_sdhost_probe(frf_sdhost *dev);
int frf_sdhost_initialize(frf_sdhost *dev);
int frf_sdhost_read_sector(frf_sdhost *dev, uint32_t lba, uint8_t buffer[512]);
int frf_sdhost_fat_read(void *context, uint32_t lba, uint8_t sector[512]);
#endif
