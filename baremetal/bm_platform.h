#ifndef BM_PLATFORM_H
#define BM_PLATFORM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { BM_CORE_MAME=0, BM_CORE_SCALE=1, BM_CORE_SCANOUT=2, BM_CORE_AUX=3 };
enum {
    BM_PAD_LEFT=1u<<0, BM_PAD_RIGHT=1u<<1, BM_PAD_UP=1u<<2, BM_PAD_DOWN=1u<<3,
    BM_PAD_B1=1u<<4, BM_PAD_B2=1u<<5, BM_PAD_START=1u<<6, BM_PAD_SELECT=1u<<7,
    BM_PAD_B3=1u<<8, BM_PAD_B4=1u<<9, BM_PAD_B5=1u<<10,
    BM_PAD_B6=1u<<11, BM_PAD_B7=1u<<12, BM_PAD_B8=1u<<13,
    BM_PAD_B9=1u<<14, BM_PAD_B10=1u<<15
};
typedef struct {
    volatile uint32_t seq, scaled_seq, done;
    const void *src;
    const uint32_t *palette;
    uint32_t palette_entries;
    uint32_t src_w, src_h, src_pitch, src_depth;
    void *dst;
    uint32_t dst_w, dst_h, dst_pitch;
    uint32_t integer_scale;
    uint32_t scanline_strength;
} bm_frame_job;
void bm_workers_init(void);
void bm_worker_loop(unsigned cpu_id);
void bm_submit_frame(const void *src, uint32_t w, uint32_t h, uint32_t pitch, uint32_t depth,
                     const uint32_t *palette, uint32_t palette_entries);
void bm_video_attach(void *fb, uint32_t width, uint32_t height, uint32_t pitch);
/* With double-virtual-height VideoCore scanout, render hidden page then flip. */
void bm_video_configure_double(uint32_t virtual_height, uint32_t allocated_size,
                                int (*flip)(unsigned page));
void bm_video_begin_game(void);
void bm_video_end_game(void);
int bm_video_is_double(void);
extern volatile uint32_t frf_bm_render_us;
extern volatile uint32_t frf_bm_scanout_us;
extern volatile uint32_t frf_bm_vsync_us;
extern volatile uint32_t frf_bm_flip_success;
extern volatile uint32_t frf_bm_flip_failure;
extern volatile uint32_t frf_bm_double_active;
extern volatile uint32_t frf_bm_present_page;

void bm_video_set_scanlines(unsigned strength);
void bm_video_set_rotation(unsigned clockwise);
void bm_video_set_orientation(unsigned flags);
void bm_debug_fill(uint16_t color);
void bm_debug_progress(unsigned stage, unsigned heartbeat);
void bm_log_init(void);
void bm_log_mark(const char *stage);
void bm_log_error_callback(int severity,const char *message);
void bm_log_tick(void);
void bm_log_end(int rc);
int bm_log_is_ready(void);
void bm_input_set_keyboard(uint8_t modifiers, const uint8_t keys[6]);
void bm_input_set_gamepad(uint32_t mask);
uint64_t bm_cycles(void);
uint64_t bm_cycles_per_second(void);
#ifdef __cplusplus
}
#endif
#endif
