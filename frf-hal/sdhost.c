/*
 * FRF HAL independent read-only BCM2837 SDHOST PIO implementation.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * Uses public BCM register locations and SD card command definitions.
 * This intentionally does not contain the Linux GPL SDHOST driver,
 * Circle SD/eMMC drivers or FatFs.
 *
 * H/W status is EXPERIMENTAL: no Pi 3 board has booted this target yet.
 * Mode: 1-bit SD, slow SDCLK divider, PIO FIFO, no SD card writes.
 * SDHOST differs from the Arasan SDHCI and is the Pi3 SD-card path
 * when Wi-Fi occupies the Arasan controller.
 */
#include "sdhost.h"
#include "hal.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define BASE FRF_SDHOST_BASE
#define SDCMD (BASE+0x00u)
#define SDARG (BASE+0x04u)
#define SDTOUT (BASE+0x08u)
#define SDCDIV (BASE+0x0Cu)
#define SDRSP0 (BASE+0x10u)
#define SDHSTS (BASE+0x20u)
#define SDVDD (BASE+0x30u)
#define SDEDM (BASE+0x34u)
#define SDHCFG (BASE+0x38u)
#define SDHBCT (BASE+0x3Cu)
#define SDDATA (BASE+0x40u)
#define SDHBLC (BASE+0x50u)
#define STATUS_ERR (0xF8u)
#define STATUS_BLOCK (0x200u)
#define STATUS_DATA_READY 1u
#define CMD_BUSY 0x8000u
#define CMD_FAIL 0x4000u
#define GPIO_BASE 0x3F200000u
#define GPIO_FSEL4 (GPIO_BASE+0x10u)
#define GPIO_FSEL5 (GPIO_BASE+0x14u)
static uint32_t read32(uint32_t a){return *(volatile uint32_t *)(uintptr_t)a;}
static void write32(uint32_t a,uint32_t v){*(volatile uint32_t *)(uintptr_t)a=v;}
static int wait_clear(uint32_t reg,uint32_t mask,uint32_t timeout_us){
    uint64_t start=frf_hal_time_us();
    while(read32(reg)&mask) {
        if(frf_hal_time_us()-start >= timeout_us)return 0;
    }
    return 1;
}
static int wait_status(uint32_t mask,uint32_t timeout_us,uint32_t *status){
    uint64_t start=frf_hal_time_us();
    for (;;) {
        uint32_t s=read32(SDHSTS);
        if(status)*status=s;
        if(s & STATUS_ERR)return 0;
        if(s & mask)return 1;
        if(frf_hal_time_us()-start >= timeout_us)return 0;
    }
}
/* GPIO48..53: independent ALT0 FSEL setup, preserve unrelated pins. */
static void setup_gpio(void) {
    uint32_t v=read32(GPIO_FSEL4);
    for(unsigned pin=48u;pin<=49u;++pin) {
        unsigned shift=(pin-40u)*3u;
        v=(v&~(7u<<shift))|(4u<<shift);
    }
    write32(GPIO_FSEL4,v);
    v=read32(GPIO_FSEL5);
    for(unsigned pin=50u;pin<=53u;++pin) {
        unsigned shift=(pin-50u)*3u;
        v=(v&~(7u<<shift))|(4u<<shift);
    }
    write32(GPIO_FSEL5,v);
    __asm__ volatile("dsb sy" ::: "memory");
}
void frf_sdhost_probe(frf_sdhost *dev) {
    if(!dev)return;
    dev->last_command_word=read32(SDCMD);
    dev->last_status=read32(SDHSTS);
    dev->last_edm=read32(SDEDM);
}
/* Returns zero on timeout/error, never blocks indefinitely. */
static int issue(frf_sdhost *dev,unsigned cmd,uint32_t arg,
                 int noresp,int longresp,int data,int crc_optional) {
    if(!wait_clear(SDCMD,CMD_BUSY,100000u))return 0;
    write32(SDHSTS,0x7F8u); /* Clear old W1C status. */
    write32(SDARG,arg);
    uint32_t word=frf_sdhost_cmd_word(cmd,noresp,longresp,data);
    dev->last_command=cmd;
    dev->last_command_word=word;
    write32(SDCMD,word);
    if(!wait_clear(SDCMD,CMD_BUSY,150000u)) {
        dev->last_status=read32(SDHSTS);
        return 0;
    }
    const uint32_t status=read32(SDHSTS);
    const uint32_t result=read32(SDCMD);
    dev->last_status=status;
    dev->last_response=read32(SDRSP0);
    dev->last_edm=read32(SDEDM);
    if((result & CMD_FAIL) || (status & STATUS_ERR)) {
        /* R3/OCR does not carry command CRC: ignore a pure CRC7 bit
           *only* if there is a response and no other controller error. */
        if(!crc_optional || noresp || (status & STATUS_ERR) != 0x10u)
            return 0;
    }
    return 1;
}
int frf_sdhost_initialize(frf_sdhost *dev) {
    if(!dev)return 0;
    memset(dev,0,sizeof(*dev));
    setup_gpio();
    write32(SDVDD,0u);
    write32(SDCMD,0u);
    write32(SDARG,0u);
    write32(SDTOUT,0x00F00000u);
    write32(SDCDIV,0x7FFu); /* slow diagnostic SD clock, core-clock dependent */
    write32(SDHSTS,0x7F8u);
    write32(SDHCFG,0u);  /* default 1-bit SD data width */
    write32(SDHBCT,512u);
    write32(SDHBLC,1u);
    /* Limit PIO FIFO threshold to 4 words on this controller. */
    uint32_t edm=read32(SDEDM);
    edm&=~((0x1Fu<<14)|(0x1Fu<<9));
    edm|=(4u<<14)|(4u<<9);
    write32(SDEDM,edm);
    frf_hal_delay_us(25000u);
    write32(SDVDD,1u);
    frf_hal_delay_us(50000u);
    if(!issue(dev,0u,0u,1,0,0,0))return 0;
    frf_hal_delay_us(3000u);
    /* SD2.0 CMD8 is optional for very old SDSC cards. */
    int v2=issue(dev,8u,0x1AAu,0,0,0,0);
    if(v2 && (dev->last_response&0xFFFu) != 0x1AAu)return 0;
    uint64_t start=frf_hal_time_us();
    uint32_t ocr=0u;
    for (;;) {
        if(!issue(dev,55u,0u,0,0,0,0))return 0;
        if(!issue(dev,41u,0x00FF8000u|(v2?(1u<<30):0u),
                  0,0,0,1))return 0;
        ocr=dev->last_response;
        if(ocr & (1u<<31))break;
        if(frf_hal_time_us()-start > 2000000u)return 0;
        frf_hal_delay_us(10000u);
    }
    dev->ocr=ocr;
    dev->high_capacity=(uint8_t)((ocr>>30)&1u);
    if(!issue(dev,2u,0u,0,1,0,0))return 0;
    if(!issue(dev,3u,0u,0,0,0,0))return 0;
    dev->rca=dev->last_response&0xFFFF0000u;
    if(!dev->rca)return 0;
    if(!issue(dev,7u,dev->rca,0,0,0,0))return 0;
    if(!dev->high_capacity) {
        if(!issue(dev,16u,512u,0,0,0,0))return 0;
    }
    /* Keep first firmware test safely slow; optimize after UART proof. */
    dev->initialized=1u;
    return 1;
}
int frf_sdhost_read_sector(frf_sdhost *dev,uint32_t lba,uint8_t buffer[512]){
    if(!dev || !dev->initialized || !buffer)return 0;
    uint32_t arg=0u;
    if(!frf_sdhost_card_arg(lba,dev->high_capacity,&arg))return 0;
    write32(SDHBCT,512u);
    write32(SDHBLC,1u);
    if(!issue(dev,17u,arg,0,0,1,0))return 0;
    for(unsigned word=0u;word<128u;++word) {
        uint32_t status=0;
        if(!wait_status(STATUS_DATA_READY,300000u,&status)){
            dev->last_status=status;
            return 0;
        }
        uint32_t value=read32(SDDATA);
        /* Always store little-endian bytes, avoid alignment/alias issues. */
        buffer[word*4u+0u]=(uint8_t)value;
        buffer[word*4u+1u]=(uint8_t)(value>>8u);
        buffer[word*4u+2u]=(uint8_t)(value>>16u);
        buffer[word*4u+3u]=(uint8_t)(value>>24u);
    }
    dev->last_status=read32(SDHSTS);
    dev->last_edm=read32(SDEDM);
    if(dev->last_status & STATUS_ERR)return 0;
    /* Clear finished-block indication if asserted. */
    if(dev->last_status & STATUS_BLOCK)write32(SDHSTS,STATUS_BLOCK);
    ++dev->sectors_read;
    return 1;
}
int frf_sdhost_fat_read(void *context,uint32_t lba,uint8_t buffer[512]){
    return frf_sdhost_read_sector((frf_sdhost *)context,lba,buffer);
}
