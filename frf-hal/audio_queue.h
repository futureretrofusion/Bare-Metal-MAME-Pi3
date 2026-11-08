/*
 * Future Retro Fusion / independent MAME stereo PCM pipeline.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 * Provides a 48 kHz SPSC queue and exact-ratio 32/48 kHz conversion.
 * This is NOT an HDMI audio driver. An independent sink is still required.
 */
#ifndef FRF_HAL_AUDIO_QUEUE_H
#define FRF_HAL_AUDIO_QUEUE_H
#include <stdint.h>
enum { FRF_AUDIO_CAPACITY = 8192 };
typedef struct {
    int16_t samples[FRF_AUDIO_CAPACITY*2u] __attribute__((aligned(64)));
    volatile uint32_t write_position;
    volatile uint32_t read_position;
    uint32_t sample_rate;
    uint32_t has_previous;
    int16_t previous_l, previous_r;
    uint64_t source_frames;
    uint64_t next_output_tick;
    uint32_t accepted_frames;
    uint32_t dropped_frames;
    uint32_t played_frames;
    uint32_t silent_frames;
} frf_audio_queue;
void frf_audio_queue_init(frf_audio_queue *q,uint32_t source_hz);
uint32_t frf_audio_queue_push(frf_audio_queue *q,
                              const int16_t *pcm_stereo,unsigned frames);
uint32_t frf_audio_queue_pull(frf_audio_queue *q,
                              int16_t *pcm_stereo,unsigned frames);
uint32_t frf_audio_queue_available(const frf_audio_queue *q);
#endif
