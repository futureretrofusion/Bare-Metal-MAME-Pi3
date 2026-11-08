/*
 * FRF HAL independent SDHOST card command encoding tests.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../sdhost.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    uint32_t arg;
    assert(frf_sdhost_cmd_word(0u,1,0,0)==0x8400u);
    assert(frf_sdhost_cmd_word(2u,0,1,0)==0x8202u);
    assert(frf_sdhost_cmd_word(17u,0,0,1)==0x8051u);
    assert(frf_sdhost_cmd_word(41u,0,0,0)==0x8029u);
    assert(frf_sdhost_card_arg(1u,1,&arg)==1 && arg==1u);
    assert(frf_sdhost_card_arg(1u,0,&arg)==1 && arg==512u);
    assert(frf_sdhost_card_arg(UINT32_MAX,1,&arg)==1 && arg==UINT32_MAX);
    assert(frf_sdhost_card_arg(UINT32_MAX,0,&arg)==0);
    assert(frf_sdhost_card_arg(1u,1,0)==0);
    puts("FRF SDHOST command and byte/block address helper tests: PASS");
    return 0;
}
