/*
 * Copyright (c) 2026 Future Retro Fusion.
 * SPDX-License-Identifier: MIT
 * Independent Pi3 USB-host diagnostic configuration, not the Circle app.
 */
#ifndef FRF_HAL_TUSB_CONFIG_H
#define FRF_HAL_TUSB_CONFIG_H
#define CFG_TUSB_MCU OPT_MCU_BCM2837
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_HOST | OPT_MODE_HIGH_SPEED)
#define CFG_TUH_ENABLED 1
#define CFG_TUH_MAX_SPEED OPT_MODE_HIGH_SPEED
#define CFG_TUH_HUB 4
#define CFG_TUH_DEVICE_MAX 16
#define CFG_TUH_ENUMERATION_BUFSIZE 1024
#define CFG_TUH_HID 8
#define CFG_TUH_MSC 0
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(64)))
#define CFG_TUSB_DEBUG 0
#define CFG_TUH_DWC2_DMA_ENABLE 1
#define CFG_TUH_DWC2_SLAVE_ENABLE 0
#define CFG_TUSB_FIFO_HWFIFO_CUSTOM_WRITE 1
#define CFG_TUSB_FIFO_HWFIFO_CUSTOM_READ 1
#define CFG_TUH_MEM_DCACHE_ENABLE 1
#define CFG_TUSB_MEM_DCACHE_LINE_SIZE 64
#endif
