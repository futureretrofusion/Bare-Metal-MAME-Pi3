/*
 * FRF HAL standalone audio clock and resampler diagnostic — NO HDMI sink.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 *
 * This proves ARM32 compilation and timer/PCM buffer code only. It does
 * not play sound, initialize VCHIQ or claim any hardware HDMI audio.
 */
#include "hal.h"
#include "audio_queue.h"
#include <stdint.h>
static frf_audio_queue q;
static int16_t source[320u*2u] __attribute__((aligned(64)));
static int16_t sink[480u*2u] __attribute__((aligned(64)));
void frf_hal_main(void) {
    frf_hal_uart_init();
    frf_hal_uart_puts("\nFRF HAL / Pi3 / software PCM queue diagnostic\n");
    frf_hal_uart_puts("NO HDMI sound backend: generated PCM is discarded.\n");
    frf_audio_queue_init(&q,32000u);
    uint32_t phase=0u;
    uint32_t n=0u;
    for(;;){
        const uint64_t begin=frf_hal_time_us();
        for(unsigned i=0u;i<320u;++i){
            int v=((int)(phase&255u)-128)*200;
            source[2u*i]=(int16_t)v;
            source[2u*i+1u]=(int16_t)-v;
            ++phase;
        }
        (void)frf_audio_queue_push(&q,source,320u);
        (void)frf_audio_queue_pull(&q,sink,480u);
        ++n;
        if(n%100u==0u){
            frf_hal_uart_puts("PCM 32->48k produced=");
            frf_hal_uart_hex32(q.accepted_frames);
            frf_hal_uart_puts(" drained=");
            frf_hal_uart_hex32(q.played_frames);
            frf_hal_uart_puts(" under=");
            frf_hal_uart_hex32(q.silent_frames);
            frf_hal_uart_puts(" over=");
            frf_hal_uart_hex32(q.dropped_frames);
            frf_hal_uart_puts("\n");
        }
        uint64_t elapsed=frf_hal_time_us()-begin;
        if(elapsed<10000ull)frf_hal_delay_us((uint32_t)(10000ull-elapsed));
    }
}
