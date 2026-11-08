#include "../baremetal/bm_platform.h"
#include <fatfs/ff.h>
#include <stdio.h>
#include <string.h>

/* Single CPU0 writer only. Reopen, flush and close for resilience to hangs.
   1 boot marker + max 240 samples at approximately 2s each. */
extern "C" {
unsigned frf_bm_cpu_pc(unsigned cpu);
unsigned frf_bm_rom_first4(void);
unsigned frf_bm_rom_fnv(void);
extern volatile unsigned frf_bm_z80_cycles;
extern volatile unsigned frf_bm_timer_fallbacks;
extern volatile unsigned frf_bm_cpu0_skips;
extern volatile unsigned frf_bm_cpu0_suspend;
extern volatile unsigned frf_bm_cpu0_last_requested;
extern volatile unsigned frf_bm_vram_writes;
extern volatile unsigned frf_bm_frames;
void bm_usb_stats(uint32_t out[7]);
extern volatile unsigned frf_bm_nonzero_sample;
extern volatile unsigned frf_bm_palette_sample;
unsigned frf_bm_vram_hash(void);
unsigned frf_bm_vram_first4(void);
void frf_bm_timer_info(unsigned *, unsigned *, unsigned *, int *, int *);
}

static int g_log_ready=0;
static int g_game_diagnostic=0;
static char g_last_error[132]={0};
extern "C" const char *bm_log_last_error(void){return g_last_error;}
extern "C" void bm_log_select_game(const char *game)
{
    /* Brief targeted bootstrap/ROM errors, no in-game periodic writes. */
    g_last_error[0]=0;
    g_game_diagnostic = game && (
        strcmp(game,"outrun")==0 || strcmp(game,"outrun2")==0 ||
        strcmp(game,"outrun1")==0 || strcmp(game,"outrunb")==0 ||
        strcmp(game,"asteroid")==0 || strcmp(game,"tmnt")==0 ||
        strcmp(game,"altbeast")==0 || strcmp(game,"blktiger")==0 ||
        strcmp(game,"mslug")==0 || strcmp(game,"mslug2")==0 ||
        strcmp(game,"mslugx")==0 || strcmp(game,"blazstar")==0 ||
        strcmp(game,"pulstar")==0);
}
static unsigned g_log_lines=0;
static unsigned g_error_lines=0;
static uint64_t g_last_tick=0;
static uint32_t g_perf_prev_frames=0;
static uint32_t g_perf_prev_render=0;
static uint32_t g_perf_prev_copy=0;
static uint32_t g_perf_prev_vsync=0;
static const char *LOGFILE="SD:/MAMELOG.TXT";

static int log_write(const char *line)
{
    if(!g_log_ready || g_log_lines>=260) return 0;
    FIL f;
    FRESULT rc=f_open(&f,LOGFILE,FA_OPEN_ALWAYS | FA_WRITE);
    if(rc!=FR_OK) return 0;
    rc=f_lseek(&f,f_size(&f));
    UINT wrote=0;
    UINT len=(UINT)strlen(line);
    if(rc==FR_OK) rc=f_write(&f,line,len,&wrote);
    if(rc==FR_OK && wrote==len) rc=f_sync(&f);
    FRESULT cr=f_close(&f);
    if(rc==FR_OK && cr==FR_OK && wrote==len) {g_log_lines++;return 1;}
    return 0;
}

#ifdef FRF_TINYUSB_MAME_HOST
extern "C" void bm_tusb_last_report(uint32_t out[5]);
extern "C" void bm_usb_stats(uint32_t out[7]);
extern "C" void bm_tusb_hw_stats(uint32_t out[7]);
extern "C" void bm_log_usb_status(void)
{
    uint32_t a[7]={0}, raw[5]={0}, hw[7]={0};
    bm_usb_stats(a);bm_tusb_last_report(raw);bm_tusb_hw_stats(hw);
    char msg[230];
    snprintf(msg,sizeof msg,
        "%u,TINYUSB_HOST_%u_KBD_%u_PAD_%u_KBREPORT_%u_PADREPORT_%u_ERROR_%u_RAW_%08x_%08x_VIDPID_%08x_LEN_%u_ID_%u\r\n",
        (unsigned)(bm_cycles()/1000ULL),
        (unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(unsigned)a[3],(unsigned)a[4],(unsigned)a[5],
        (unsigned)raw[2],(unsigned)raw[3],(unsigned)raw[0],(unsigned)raw[1],(unsigned)raw[4]);
    log_write(msg);
    snprintf(msg,sizeof msg,
        "%u,TINYUSB_POWER_%u_STATE_%u_DWC2_%08x_HPRT_%08x_DEVICE_%u_ENUMOK_%u_ENUMERR_%u\r\n",
        (unsigned)(bm_cycles()/1000ULL),
        (unsigned)hw[0],(unsigned)hw[1],(unsigned)hw[2],(unsigned)hw[3],
        (unsigned)hw[4],(unsigned)hw[5],(unsigned)hw[6]);
    log_write(msg);
}
#endif
extern "C" int bm_log_is_ready(void) { return g_log_ready; }

extern "C" void bm_log_error_callback(int severity,const char *message)
{
    if(!message || !*message)return;
    /* Capture failures for MAMERUN.CSV even if normal gameplay logging is off. */
    if(severity==1 || strstr(message,"Out of timers") ||
       strstr(message,"failed") || strstr(message,"ERROR:") ||
       strstr(message,"NOT FOUND") || strstr(message,"Unable to")) {
        size_t p=0;
        for(;message[p] && p<sizeof(g_last_error)-1;p++){
            char c=message[p];
            if(c=='\r'||c=='\n')break;
            g_last_error[p]=(c==',')?';':c;
        }
        g_last_error[p]=0;
    }
    if(!g_log_ready || severity<1 || g_error_lines>=40)return;
    char clean[134];
    unsigned i=0;
    for(;message[i] && i<sizeof(clean)-1;i++) {
        char c=message[i];
        if(c=='\n'||c=='\r') break;
        clean[i]=(c==','||c=='"')?';':((unsigned char)c<32?' ':c);
    }
    clean[i]=0;
    if(!i) return;
    char line[200];
    snprintf(line,sizeof line,"%u,MAME_MSG_%d_%s\r\n",
             (unsigned)(bm_cycles()/1000ULL),severity,clean);
    if(log_write(line)) ++g_error_lines;
}
extern "C" void bm_log_init(void)
{
#ifdef FRF_MAME_FAST
    if(!g_game_diagnostic){
        g_log_ready=0;
        return; /* release: no per-game logging except targeted debug titles */
    }
#endif
    FIL f;
    FRESULT rc=f_open(&f,LOGFILE,FA_CREATE_ALWAYS | FA_WRITE);
    g_log_ready=(rc==FR_OK);
    if(g_log_ready) {
#ifdef FRF_TINYUSB_MAME_HOST
        const char head[]="FRF MAME106RGB PI3 v0.52 DIRECT INTEGER RENDER\r\n"
#else
        const char head[]="FRF MAME106RGB PI3 v0.48 CIRCLE USB HOTPLUG RECOVERY\r\n"
#endif
          "time_ms,stage,z80cycles,vramwrites,vramhash,vramfirst4,frames,pixels_nonzero,palette1,timers,enabled,due,base_seconds,next_seconds,fallbacks,cpu0_skips,cpu0_suspend,cpu0_requested,cpu0_pc,cpu1_pc,rom_first4,rom_fnv,render_us,scanout_us,vsync_us,flip_ok,flip_fail,double_active,present_page,usb_polls,kbd_reattach,pad_reattach,kbd_reports,pad_reports,kbd_removed,pad_removed\r\n";
        UINT n=0;
        FRESULT wr=f_write(&f,head,sizeof(head)-1,&n);
        FRESULT sy=f_sync(&f);
        FRESULT cl=f_close(&f);
        g_log_ready=(wr==FR_OK && sy==FR_OK && cl==FR_OK && n==sizeof(head)-1);
    }
    g_log_lines=0;
    g_error_lines=0;
    g_last_tick=bm_cycles();
    g_perf_prev_frames=frf_bm_frames;
    g_perf_prev_render=frf_bm_render_us;
    g_perf_prev_copy=frf_bm_scanout_us;
    g_perf_prev_vsync=frf_bm_vsync_us;
}

extern "C" void bm_log_mark(const char *stage)
{
    if(!g_log_ready) return;
    char buf[190];
    unsigned tms=(unsigned)(bm_cycles()/1000ULL);
    snprintf(buf,sizeof(buf),"%u,%s,0,0,0,0,0,0,0,0,0,0,0,0\r\n",
             tms, stage ? stage : "UNKNOWN");
    log_write(buf);
}

extern "C" void bm_system_poll(void);
extern "C" void bm_log_tick(void)
{
#ifdef FRF_MAME_FAST
    /* O(1) fast path: service reboot/hotplug periodically without SD IO. */
    static unsigned fast_polls=0;
    if((++fast_polls & 31u)==0) bm_system_poll();
    return;
#else
    bm_system_poll();
    if(!g_log_ready || g_log_lines>=250) return;
    uint64_t now=bm_cycles();
    if(now-g_last_tick<2000000ULL) return;
    const uint64_t elapsed=now-g_last_tick;
    g_last_tick=now;
    uint32_t frames=frf_bm_frames, dframes=frames-g_perf_prev_frames;
    uint32_t render=frf_bm_render_us, copy=frf_bm_scanout_us, vsync=frf_bm_vsync_us;
    uint32_t drender=render-g_perf_prev_render;
    uint32_t dcopy=copy-g_perf_prev_copy;
    uint32_t dvsync=vsync-g_perf_prev_vsync;
    g_perf_prev_frames=frames;
    g_perf_prev_render=render;
    g_perf_prev_copy=copy;
    g_perf_prev_vsync=vsync;
    /* fps_x100 is the measured presented-frame rate, not the emulation
       game's declared refresh. Compare against native 60Hz in these titles. */
    char perf[210];
    snprintf(perf,sizeof perf,
        "%u,PERF_PRESENT_FPSx100_%u_FRAMES_%u_RENDERavgUS_%u_COPYavgUS_%u_VSYNCavgUS_%u_DOUBLE_%u\r\n",
        (unsigned)(now/1000ULL),
        (unsigned)((uint64_t)dframes*100000000ULL/elapsed),(unsigned)dframes,
        (unsigned)(dframes?drender/dframes:0),(unsigned)(dframes?dcopy/dframes:0),
        (unsigned)(dframes?dvsync/dframes:0),(unsigned)frf_bm_double_active);
    log_write(perf);
    unsigned total=0,enabled=0,due=0;
    int bas=0,next=-1;
    frf_bm_timer_info(&total,&enabled,&due,&bas,&next);
    uint32_t usb[7]={0};
    bm_usb_stats(usb);
    char line[340];
    snprintf(line,sizeof(line),
        "%u,RUN,%u,%u,%08x,%08x,%u,%u,%08x,%u,%u,%u,%d,%d,%u,%u,%u,%u,%08x,%08x,%08x,%08x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
        (unsigned)(now/1000ULL),
        (unsigned)frf_bm_z80_cycles,(unsigned)frf_bm_vram_writes,
        frf_bm_vram_hash(),frf_bm_vram_first4(),
        (unsigned)frf_bm_frames,(unsigned)frf_bm_nonzero_sample,
        (unsigned)frf_bm_palette_sample,
        total,enabled,due,bas,next,
        (unsigned)frf_bm_timer_fallbacks,(unsigned)frf_bm_cpu0_skips,
        (unsigned)frf_bm_cpu0_suspend,(unsigned)frf_bm_cpu0_last_requested,
        frf_bm_cpu_pc(0),frf_bm_cpu_pc(1),frf_bm_rom_first4(),frf_bm_rom_fnv(),
        (unsigned)frf_bm_render_us,(unsigned)frf_bm_scanout_us,
        (unsigned)frf_bm_vsync_us,
        (unsigned)frf_bm_flip_success,(unsigned)frf_bm_flip_failure,
        (unsigned)frf_bm_double_active,(unsigned)frf_bm_present_page,
        (unsigned)usb[0],(unsigned)usb[1],(unsigned)usb[2],(unsigned)usb[3],(unsigned)usb[4],(unsigned)usb[5],(unsigned)usb[6]);
    log_write(line);
#endif
}

extern "C" void bm_log_end(int rc)
{
    if(!g_log_ready) return;
    char buf[100];
    snprintf(buf,sizeof(buf),"%u,EXIT_%d,0,0,0,0,0,0,0,0,0,0,0,0\r\n",
             (unsigned)(bm_cycles()/1000ULL),rc);
    log_write(buf);
}
