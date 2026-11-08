#ifndef FRF_PERF_PROFILE_H
#define FRF_PERF_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Last completed window (120 native arcade frames). Timings are average us/frame.
   Generated in memory only: no per-frame SD logging. */
typedef struct frf_perf_snapshot
{
    unsigned valid, frames;
    unsigned long core_us, update_us, sound_us, video_us, osd_us;
    unsigned long palette_us, partial_updates;
} frf_perf_snapshot;
void frf_perf_get_snapshot(frf_perf_snapshot *out);

void frf_perf_frame_begin(void);
void frf_perf_frame_end(void);
void frf_perf_sound_begin(void);
void frf_perf_sound_end(void);
void frf_perf_video_begin(void);
void frf_perf_video_end(void);
void frf_perf_note_partial_update(void);
void frf_perf_osd_begin(void);
void frf_perf_osd_end(void);
void frf_perf_palette_begin(void);
void frf_perf_palette_end(void);
void frf_perf_remap_begin(void);
void frf_perf_remap_end(void);
void frf_perf_c2p_begin(void);
void frf_perf_c2p_end(void);
void frf_perf_wait_begin(void);
void frf_perf_wait_end(void);

#ifdef __cplusplus
}
#endif

#endif
