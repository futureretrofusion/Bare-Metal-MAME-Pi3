/*
 * FRF HAL SDHOST controller command/PIO sequence with fake MMIO registers.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * This validates the real sdhost.c control flow, not physical Pi3 wiring.
 */
#include "../sdhost.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define SD_BASE FRF_SDHOST_BASE
#define GPIO_BASE 0x3F200000u
enum { MOCK_SDHC=0, MOCK_SDSC=1, MOCK_BAD_RCA=2, MOCK_HANG=3 };
static int scenario;
static int data_error;
static uint64_t clock_us;
static uint32_t command_reg,argument_reg,response_reg,status_reg,edm_reg;
static uint32_t gpio4,gpio5, vdd,cdiv,blksize,blockcount;
static unsigned commands[64],command_count;
static int data_live;
static unsigned words_read;
static uint32_t current_lba, last_read_arg;
static int fail_after_n;
static void reset(int scenario_value) {
    scenario=scenario_value;
    data_error=0;
    clock_us=0u;
    command_reg=argument_reg=response_reg=status_reg=edm_reg=0u;
    gpio4=gpio5=vdd=cdiv=blksize=blockcount=0u;
    command_count=0u; data_live=0;words_read=0u;
    current_lba=last_read_arg=0u;
    fail_after_n=20;
}
uint64_t frf_hal_time_us(void) {clock_us+=100u;return clock_us;}
void frf_hal_delay_us(uint32_t delay){clock_us+=delay;}
uint32_t frf_sdhost_mock_read32(uint32_t addr){
    if(addr==GPIO_BASE+0x10u)return gpio4;
    if(addr==GPIO_BASE+0x14u)return gpio5;
    if(addr==SD_BASE+0x00u) {
        if(scenario==MOCK_HANG && command_count && commands[command_count-1u]==8u)
            return 0x8000u;
        return command_reg;
    }
    if(addr==SD_BASE+0x04u)return argument_reg;
    if(addr==SD_BASE+0x0Cu)return cdiv;
    if(addr==SD_BASE+0x10u)return response_reg;
    if(addr==SD_BASE+0x20u) {
        if(data_live && data_error && words_read >= (unsigned)fail_after_n)
            return status_reg | 0x40u;
        return status_reg | (data_live?1u:0u);
    }
    if(addr==SD_BASE+0x30u)return vdd;
    if(addr==SD_BASE+0x34u)return edm_reg;
    if(addr==SD_BASE+0x3Cu)return blksize;
    if(addr==SD_BASE+0x50u)return blockcount;
    if(addr==SD_BASE+0x40u) {
        assert(data_live && words_read<128u);
        uint32_t result=(current_lba<<16)|(uint32_t)words_read;
        ++words_read;
        if(words_read>=128u){data_live=0;status_reg|=0x200u;}
        return result;
    }
    return 0u;
}
void frf_sdhost_mock_write32(uint32_t addr,uint32_t v) {
    if(addr==GPIO_BASE+0x10u){gpio4=v;return;}
    if(addr==GPIO_BASE+0x14u){gpio5=v;return;}
    if(addr==SD_BASE+0x04u){argument_reg=v;return;}
    if(addr==SD_BASE+0x08u)return;
    if(addr==SD_BASE+0x0Cu){cdiv=v;return;}
    if(addr==SD_BASE+0x20u){status_reg &= ~v;return;}
    if(addr==SD_BASE+0x30u){vdd=v;return;}
    if(addr==SD_BASE+0x34u){edm_reg=v;return;}
    if(addr==SD_BASE+0x38u)return;
    if(addr==SD_BASE+0x3Cu){blksize=v;return;}
    if(addr==SD_BASE+0x50u){blockcount=v;return;}
    if(addr==SD_BASE+0x00u) {
        command_reg = v & ~0x8000u;
        if(!(v & 0x8000u))return; /* controller reset */
        unsigned cmd=v & 63u;
        assert(cmd!=24u && cmd!=25u); /* no card-write commands ever sent */
        assert(command_count<64u);
        commands[command_count++]=cmd;
        response_reg=0u;
        if(cmd==8u){
            if(scenario==MOCK_SDSC) {
                command_reg|=0x4000u;
                status_reg|=0x40u;
            } else response_reg=0x1AAu;
        } else if(cmd==41u) {
            response_reg=(scenario==MOCK_SDSC?0x80FF8000u:0xC0FF8000u);
            /* R3 replies are non-CRC: mock a harmless CRC7 indication. */
            status_reg|=0x10u;
            command_reg|=0x4000u;
        } else if(cmd==3u) {
            response_reg=scenario==MOCK_BAD_RCA?0u:0x12340000u;
        } else if(cmd==17u){
            last_read_arg=argument_reg;
            current_lba=(scenario==MOCK_SDSC)?argument_reg/512u:argument_reg;
            words_read=0u;
            data_live=1;
        }
        return;
    }
    assert(0 && "unexpected SDHOST MMIO register write");
}
static void check_gpio(void) {
    assert(((gpio4>>24u)&7u)==4u); /* GPIO48 ALT0 */
    assert(((gpio4>>27u)&7u)==4u); /* GPIO49 ALT0 */
    for(unsigned i=0;i<4u;++i)assert(((gpio5>>(i*3u))&7u)==4u);
}
static void check_sequence(int mode) {
    const unsigned expected[]={0,8,55,41,2,3,7};
    assert(command_count>=7u);
    for(unsigned i=0;i<7u;++i)assert(commands[i]==expected[i]);
    if(mode==MOCK_SDSC){assert(commands[7]==16u);}
    else assert(command_count==7u);
}
int main(void) {
    frf_sdhost sd;
    uint8_t output[512];
    reset(MOCK_SDHC);
    assert(frf_sdhost_initialize(&sd));
    assert(sd.initialized && sd.rca==0x12340000u && sd.high_capacity);
    assert(vdd==1u && cdiv==0x7FFu && blksize==512u && blockcount==1u);
    check_gpio();check_sequence(MOCK_SDHC);
    assert(frf_sdhost_read_sector(&sd,42u,output));
    assert(last_read_arg==42u && sd.sectors_read==1u);
    assert(output[0]==0u && output[1]==0u && output[2]==42u);
    assert(output[508]==127u && output[510]==42u);
    assert(commands[7]==17u);
    data_error=1;
    assert(!frf_sdhost_read_sector(&sd,43u,output));
    assert(sd.sectors_read==1u);
    reset(MOCK_SDSC);
    assert(frf_sdhost_initialize(&sd));
    assert(sd.initialized && !sd.high_capacity);
    check_sequence(MOCK_SDSC);
    assert(frf_sdhost_read_sector(&sd,42u,output));
    assert(last_read_arg==42u*512u && sd.sectors_read==1u);
    reset(MOCK_BAD_RCA);
    assert(!frf_sdhost_initialize(&sd));
    assert(sd.last_command==3u && !sd.initialized);
    reset(MOCK_HANG);
    assert(!frf_sdhost_initialize(&sd));
    assert(sd.last_command==8u);
    assert(clock_us<1000000u);
    puts("FRF SDHOST full command sequence, SDHC/SDSC, PIO, errors and timeouts: PASS");
    return 0;
}
