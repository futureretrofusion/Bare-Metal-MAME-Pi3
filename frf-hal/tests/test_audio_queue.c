/*
 * FRF HAL audio SPSC queue and exact 32k/48k PCM tests.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include "../audio_queue.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static frf_audio_queue q;
static frf_audio_queue q2;
static int16_t huge[FRF_AUDIO_CAPACITY*2u];
static int16_t out[(FRF_AUDIO_CAPACITY+2u)*2u];
int main(void){
    frf_audio_queue_init(&q,48000u);
    const int16_t source[]={-100,100,-50,50,0,0,500,-500};
    assert(frf_audio_queue_push(&q,source,4u)==4u);
    assert(frf_audio_queue_available(&q)==4u);
    assert(frf_audio_queue_pull(&q,out,6u)==4u);
    assert(!memcmp(source,out,8u*sizeof(int16_t)));
    assert(out[8]==0 && out[11]==0);
    assert(q.played_frames==4u && q.silent_frames==2u);
    assert(frf_audio_queue_available(&q)==0u);
    frf_audio_queue_init(&q,32000u);
    frf_audio_queue_init(&q2,32000u);
    const int16_t ramp[]={0,0,300,-300,600,-600,900,-900};
    assert(frf_audio_queue_push(&q,ramp,4u)==5u);
    assert(frf_audio_queue_push(&q2,ramp,2u)==2u);
    assert(frf_audio_queue_push(&q2,ramp+4u,2u)==3u);
    assert(frf_audio_queue_available(&q)==5u);
    assert(frf_audio_queue_available(&q2)==5u);
    int16_t result[12]={0};
    assert(frf_audio_queue_pull(&q,result,6u)==5u);
    assert(result[0]==0 && result[1]==0);
    assert(result[2]==200 && result[3]==-200);
    assert(result[4]==400 && result[5]==-400);
    assert(result[6]==600 && result[7]==-600);
    assert(result[8]==800 && result[9]==-800);
    assert(result[10]==0 && result[11]==0);
    int16_t other[12]={0};
    assert(frf_audio_queue_pull(&q2,other,6u)==5u);
    assert(!memcmp(result,other,sizeof(result)));
    assert(q.next_output_tick==q2.next_output_tick);
    frf_audio_queue_init(&q,48000u);
    for(unsigned i=0u;i<FRF_AUDIO_CAPACITY;++i) {
        huge[2u*i]=(int16_t)i;
        huge[2u*i+1u]=-(int16_t)i;
    }
    assert(frf_audio_queue_push(&q,huge,FRF_AUDIO_CAPACITY)==FRF_AUDIO_CAPACITY);
    assert(frf_audio_queue_push(&q,huge,2u)==0u);
    assert(q.dropped_frames==2u);
    assert(frf_audio_queue_available(&q)==FRF_AUDIO_CAPACITY);
    assert(frf_audio_queue_pull(&q,out,FRF_AUDIO_CAPACITY+1u)==FRF_AUDIO_CAPACITY);
    assert(!memcmp(huge,out,sizeof(huge)));
    assert(out[FRF_AUDIO_CAPACITY*2u]==0);
    assert(frf_audio_queue_available(&q)==0u);
    assert(frf_audio_queue_push(&q,huge,1000u)==1000u);
    assert(frf_audio_queue_pull(&q,out,1000u)==1000u);
    assert(!memcmp(huge,out,1000u*2u*sizeof(int16_t)));
    assert(q.played_frames==FRF_AUDIO_CAPACITY+1000u);
    puts("FRF audio: 48k PCM, 32k->48k continuity, SPSC FIFO, underrun/overflow PASS");
    return 0;
}
