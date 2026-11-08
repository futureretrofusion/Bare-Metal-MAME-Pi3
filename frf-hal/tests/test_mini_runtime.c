/*
 * FRF HAL diagnostics-only libc formatting test, native host.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    char b[64];
    int n=snprintf(b,sizeof(b),"%s: %03u / %04X","FRF",7u,0xABCDu);
    assert(n == 15);
    assert(strcmp(b,"FRF: 007 / ABCD") == 0);
    n=snprintf(b,sizeof(b),"%u.%03u Hz",59u,940u);
    assert(n == 9);
    assert(strcmp(b,"59.940 Hz")==0);
    n=snprintf(b,sizeof(b),"%d / %% / %c",-12,'Q');
    assert(n == 11);
    assert(strcmp(b,"-12 / % / Q")==0);
    volatile size_t truncated_capacity=4u;
    n=snprintf(b,truncated_capacity,"%s","abcdef");
    assert(n == 6 && strcmp(b,"abc")==0);
    n=snprintf(NULL,0,"x%u",123u);
    assert(n == 4);
    puts("FRF HAL limited diagnostic snprintf: PASS");
    return 0;
}
