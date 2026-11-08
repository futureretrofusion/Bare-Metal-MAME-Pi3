/*
 * FRF HAL SPSC audio ring concurrent CPU host stress test.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Producer and consumer use actual pthread threads (not Pi3 cores).
 */
#include "../audio_queue.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
enum { BATCH=32u, TOTAL=32768u };
static frf_audio_queue queue;
static volatile int problem;
static void *producer(void *arg) {
    (void)arg;
    int16_t buf[BATCH*2u];
    for(uint32_t first=0u;first<TOTAL;first+=BATCH){
        while(frf_audio_queue_available(&queue)>FRF_AUDIO_CAPACITY-BATCH) {}
        for(unsigned i=0u;i<BATCH;++i){
            uint32_t n=first+i;
            buf[2u*i]=(int16_t)n;
            buf[2u*i+1u]=(int16_t)(~n);
        }
        if(frf_audio_queue_push(&queue,buf,BATCH)!=BATCH)
            __atomic_store_n(&problem,1,__ATOMIC_RELAXED);
    }
    return 0;
}
static void *consumer(void *arg) {
    (void)arg;
    int16_t buf[BATCH*2u];
    for(uint32_t first=0u;first<TOTAL;first+=BATCH){
        while(frf_audio_queue_available(&queue)<BATCH) {}
        if(frf_audio_queue_pull(&queue,buf,BATCH)!=BATCH)
            __atomic_store_n(&problem,1,__ATOMIC_RELAXED);
        for(unsigned i=0u;i<BATCH;++i){
            uint32_t n=first+i;
            if(buf[2u*i]!=(int16_t)n || buf[2u*i+1u]!=(int16_t)(~n))
                __atomic_store_n(&problem,1,__ATOMIC_RELAXED);
        }
    }
    return 0;
}
int main(void){
    frf_audio_queue_init(&queue,48000u);
    pthread_t one,two;
    assert(pthread_create(&one,0,producer,0)==0);
    assert(pthread_create(&two,0,consumer,0)==0);
    assert(pthread_join(one,0)==0);
    assert(pthread_join(two,0)==0);
    assert(!__atomic_load_n(&problem,__ATOMIC_RELAXED));
    assert(queue.dropped_frames==0u && queue.silent_frames==0u);
    assert(queue.accepted_frames==TOTAL && queue.played_frames==TOTAL);
    assert(frf_audio_queue_available(&queue)==0u);
    puts("FRF SPSC audio 32,768-frame cross-thread delivery: PASS");
    return 0;
}
