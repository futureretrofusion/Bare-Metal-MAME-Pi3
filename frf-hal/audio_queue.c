/*
 * FRF HAL stereo audio queue and 32k->48k linear resampler.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * No Circle, VCHIQ, malloc, float, stdlib or DMA dependency.
 * Producer and consumer have single ownership of write/read indexes.
 * Memory is currently uncached in diagnostic images.
 */
#include "audio_queue.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static void push_one(frf_audio_queue *q,int16_t l,int16_t r) {
    uint32_t write=__atomic_load_n(&q->write_position,__ATOMIC_RELAXED);
    uint32_t read=__atomic_load_n(&q->read_position,__ATOMIC_ACQUIRE);
    if((uint32_t)(write-read)>=FRF_AUDIO_CAPACITY) {
        ++q->dropped_frames;
        return;
    }
    unsigned at=(write % FRF_AUDIO_CAPACITY)*2u;
    q->samples[at]=l;
    q->samples[at+1u]=r;
    __atomic_store_n(&q->write_position,write+1u,__ATOMIC_RELEASE);
    ++q->accepted_frames;
}
void frf_audio_queue_init(frf_audio_queue *q,uint32_t rate) {
    if(!q)return;
    memset(q,0,sizeof(*q));
    q->sample_rate=(rate==32000u)?32000u:48000u;
}
uint32_t frf_audio_queue_push(frf_audio_queue *q,
                              const int16_t *pcm,unsigned count) {
    if(!q || !pcm || count==0u)return 0u;
    const uint32_t accepted_before=q->accepted_frames;
    if(q->sample_rate==48000u) {
        for(unsigned i=0u;i<count;++i)push_one(q,pcm[2u*i],pcm[2u*i+1u]);
        return q->accepted_frames-accepted_before;
    }
    /* Source frames occupy successive 3-tick intervals. Output frames
       occupy successive 2-tick intervals (48 kHz / 32 kHz = 3/2).
       Preserve phase and previous input sample across MAME frame calls.
       64-bit ticks avoid fractional-frequency pitch drift. */
    for(unsigned i=0u;i<count;++i){
        const int16_t l=pcm[2u*i],r=pcm[2u*i+1u];
        if(!q->has_previous) {
            push_one(q,l,r);
            q->previous_l=l;
            q->previous_r=r;
            q->has_previous=1u;
            q->next_output_tick=2u;
            continue;
        }
        ++q->source_frames;
        const uint64_t end=3u*q->source_frames;
        while(q->next_output_tick<=end){
            const unsigned b=(unsigned)(q->next_output_tick-(end-3u));
            const unsigned a=3u-b;
            const int32_t ol=((int32_t)q->previous_l*(int32_t)a +
                              (int32_t)l*(int32_t)b)/3;
            const int32_t rr=((int32_t)q->previous_r*(int32_t)a +
                              (int32_t)r*(int32_t)b)/3;
            push_one(q,(int16_t)ol,(int16_t)rr);
            q->next_output_tick+=2u;
        }
        q->previous_l=l;
        q->previous_r=r;
    }
    return q->accepted_frames-accepted_before;
}
uint32_t frf_audio_queue_available(const frf_audio_queue *q) {
    if(!q)return 0u;
    const uint32_t write=__atomic_load_n(&q->write_position,__ATOMIC_ACQUIRE);
    const uint32_t read=__atomic_load_n(&q->read_position,__ATOMIC_ACQUIRE);
    const uint32_t used=write-read;
    return used<=FRF_AUDIO_CAPACITY?used:FRF_AUDIO_CAPACITY;
}
uint32_t frf_audio_queue_pull(frf_audio_queue *q,int16_t *out,unsigned count) {
    if(!q || !out || count==0u)return 0u;
    const uint32_t write=__atomic_load_n(&q->write_position,__ATOMIC_ACQUIRE);
    uint32_t read=__atomic_load_n(&q->read_position,__ATOMIC_RELAXED);
    const uint32_t available=write-read;
    unsigned real=count;
    if(real>available)real=available;
    if(real>FRF_AUDIO_CAPACITY)real=FRF_AUDIO_CAPACITY;
    for(unsigned i=0u;i<real;++i) {
        unsigned at=((read+i)%FRF_AUDIO_CAPACITY)*2u;
        out[2u*i]=q->samples[at];
        out[2u*i+1u]=q->samples[at+1u];
    }
    __atomic_store_n(&q->read_position,read+real,__ATOMIC_RELEASE);
    for(unsigned i=real;i<count;++i) {
        out[2u*i]=0;
        out[2u*i+1u]=0;
    }
    q->played_frames+=real;
    q->silent_frames+=count-real;
    return real;
}
