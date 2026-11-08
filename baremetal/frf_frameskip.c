/* Native MAME emulation-frame skipper. CPU and audio remain active. */
#include <stdint.h>
#include "bm_platform.h"
static unsigned mode=0,level=0,target_hz=60000;
static unsigned frame_counter=0,frames_skipped=0,frames_drawn=0;
static unsigned window_frames=0,fast_windows=0;
static unsigned decision=0;
static uint64_t window_begin=0;
static unsigned last_measured_hz=0;
static uint64_t last_frame_finish=0;
static unsigned window_late=0,late_consecutive=0;
void frf_skip_start(unsigned selected_mode,unsigned native_millihz)
{
 mode=selected_mode>5?0:selected_mode;
 level=(mode>=2)?mode-1:0;
 target_hz=(native_millihz>=20000u && native_millihz<=240000u)?native_millihz:60000u;
 frame_counter=frames_skipped=frames_drawn=0;
 last_frame_finish=0;window_late=late_consecutive=0;
 window_frames=fast_windows=decision=0;window_begin=0;last_measured_hz=0;
}
void frf_skip_frame_begin(void)
{
 uint64_t now=bm_cycles();
 if(!window_begin)window_begin=now;
 if(++window_frames>=60u){
  uint64_t elapsed=now-window_begin;
  if(elapsed){
   uint64_t hz=(uint64_t)window_frames*1000000000ULL/elapsed;
   last_measured_hz=(unsigned)(hz>0xffffffffULL?0xffffffffu:hz);
   if(mode==1){
    if(hz*100u<(uint64_t)target_hz*92u){
     fast_windows=0;if(level<3u)level++;
    }else if(hz*100u>(uint64_t)target_hz*98u){
     if(++fast_windows>=3u && level>0){level--;fast_windows=0;}
    }else fast_windows=0;
   }
  }
  window_begin=now;window_frames=0;
 }
 decision=(level && frame_counter>0u && frame_counter%(level+1u)!=0u)?1u:0u;
 frame_counter++;
 if(decision)frames_skipped++;else frames_drawn++;
}
int frf_skip_this_frame(void){return (int)decision;}
unsigned frf_skip_level(void){return level;}
unsigned frf_skip_measured_hz(void){return last_measured_hz;}
unsigned frf_skip_drawn(void){return frames_drawn;}
unsigned frf_skip_dropped(void){return frames_skipped;}

/* Pacing: video flip normally waits for VBlank on drawn frames. When
   skipping the flip, explicitly pace the emulated frame against native
   refresh. This does not artificially slow a frame that exceeded budget. */
void frf_skip_frame_finish(void)
{
 if(!mode) return;
 uint64_t now=bm_cycles();
 uint64_t period=1000000000ULL/(uint64_t)target_hz;
 if(period<4000u)period=4000u;
 if(!last_frame_finish){last_frame_finish=now;return;}
 uint64_t due=last_frame_finish+period;
 if(due>now && due-now<30000ULL){
  while(bm_cycles()<due)__asm__ volatile("yield");
  now=bm_cycles();
 }
 if(now>due+(period>>2))window_late++;
 last_frame_finish=now;
}
