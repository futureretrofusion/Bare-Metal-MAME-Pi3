extern "C" {
#include "mamecore.h"
#include "mame.h"
#include "video.h"
#include "osdepend.h"
#include "fileio.h"
#include "input.h"
extern rgb_t *game_palette;
int palette_get_total_colors_with_ui(void);
}
#include "bm_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

extern "C" { volatile unsigned frf_bm_native_refresh_millihz=0; }
static int g_master_volume = 0;
static int g_sound_enabled = 1;
static volatile uint32_t g_keymask = 0;
static volatile uint32_t g_padmask = 0;
/* USB keyboard HID modifiers: separate from packed 32-bit game keymap. */
static volatile uint8_t g_altmask = 0;

enum {
    OSD_K_LEFT=1, OSD_K_RIGHT, OSD_K_UP, OSD_K_DOWN,
    OSD_K_CTRL, OSD_K_LALT, OSD_K_RALT,
    OSD_K_1, OSD_K_2, OSD_K_5, OSD_K_ESC,
    OSD_K_SPACE, OSD_K_TAB, OSD_K_ENTER, OSD_K_F2, OSD_K_F3,
    OSD_K_PAUSE, OSD_K_SHIFT, OSD_K_Z, OSD_K_X, OSD_K_C, OSD_K_V, OSD_K_B,
    OSD_K_W, OSD_K_A, OSD_K_S, OSD_K_D, OSD_K_3, OSD_K_4,
    OSD_K_6, OSD_K_7, OSD_K_8, OSD_K_9, OSD_K_0,
    OSD_J_LEFT, OSD_J_RIGHT, OSD_J_UP, OSD_J_DOWN,
    OSD_J_B1, OSD_J_B2, OSD_J_B3, OSD_J_B4, OSD_J_B5,
    OSD_J_B6, OSD_J_B7, OSD_J_B8, OSD_J_B9, OSD_J_B10,
    OSD_J_START, OSD_J_SELECT
};

static const os_code_info g_codes[] = {
    {"Left", OSD_K_LEFT, KEYCODE_LEFT}, {"Right", OSD_K_RIGHT, KEYCODE_RIGHT},
    {"Up", OSD_K_UP, KEYCODE_UP}, {"Down", OSD_K_DOWN, KEYCODE_DOWN},
    {"Left Ctrl", OSD_K_CTRL, KEYCODE_LCONTROL},
    {"Left Alt",OSD_K_LALT,KEYCODE_LALT},
    {"Right Alt",OSD_K_RALT,KEYCODE_RALT},
    {"1", OSD_K_1, KEYCODE_1}, {"2", OSD_K_2, KEYCODE_2}, {"5", OSD_K_5, KEYCODE_5},
    {"Escape", OSD_K_ESC, KEYCODE_ESC},
    {"Space", OSD_K_SPACE, KEYCODE_SPACE},
    {"Tab",OSD_K_TAB,KEYCODE_TAB}, {"Enter",OSD_K_ENTER,KEYCODE_ENTER},
    {"F2 (Service)",OSD_K_F2,KEYCODE_F2}, {"F3 (Reset)",OSD_K_F3,KEYCODE_F3},
    {"P (Pause)",OSD_K_PAUSE,KEYCODE_P}, {"Shift",OSD_K_SHIFT,KEYCODE_LSHIFT},
    {"Z",OSD_K_Z,KEYCODE_Z},{"X",OSD_K_X,KEYCODE_X},
    {"C",OSD_K_C,KEYCODE_C},{"V",OSD_K_V,KEYCODE_V},{"B",OSD_K_B,KEYCODE_B},
    {"W",OSD_K_W,KEYCODE_W},{"A",OSD_K_A,KEYCODE_A},
    {"S",OSD_K_S,KEYCODE_S},{"D",OSD_K_D,KEYCODE_D},
    {"3",OSD_K_3,KEYCODE_3},{"4",OSD_K_4,KEYCODE_4},
    {"6",OSD_K_6,KEYCODE_6},{"7",OSD_K_7,KEYCODE_7},
    {"8",OSD_K_8,KEYCODE_8},{"9",OSD_K_9,KEYCODE_9},
    {"0",OSD_K_0,KEYCODE_0},
    {"P1 Left", OSD_J_LEFT, JOYCODE_1_LEFT}, {"P1 Right", OSD_J_RIGHT, JOYCODE_1_RIGHT},
    {"P1 Up", OSD_J_UP, JOYCODE_1_UP}, {"P1 Down", OSD_J_DOWN, JOYCODE_1_DOWN},
    {"P1 Button 1", OSD_J_B1, JOYCODE_1_BUTTON1}, {"P1 Button 2", OSD_J_B2, JOYCODE_1_BUTTON2},
    {"P1 Button 3",OSD_J_B3,JOYCODE_1_BUTTON3},
    {"P1 Button 4",OSD_J_B4,JOYCODE_1_BUTTON4},
    {"P1 Button 5",OSD_J_B5,JOYCODE_1_BUTTON5},
    {"P1 Button 6",OSD_J_B6,JOYCODE_1_BUTTON6},
    {"P1 Button 7",OSD_J_B7,JOYCODE_1_BUTTON7},
    {"P1 Button 8",OSD_J_B8,JOYCODE_1_BUTTON8},
    {"P1 Button 9",OSD_J_B9,JOYCODE_1_BUTTON9},
    {"P1 Button 10",OSD_J_B10,JOYCODE_1_BUTTON10},
    {"P1 Start", OSD_J_START, JOYCODE_1_START}, {"P1 Select", OSD_J_SELECT, JOYCODE_1_SELECT},
    {0,0,CODE_NONE}
};

static const char *base_for_type(int t)
{
    switch (t) {
        case FILETYPE_ROM: return "roms";
        case FILETYPE_SAMPLE: return "samples";
        case FILETYPE_ARTWORK: return "artwork";
        case FILETYPE_NVRAM: return "nvram";
        case FILETYPE_HIGHSCORE: return "hi";
        case FILETYPE_CONFIG: return "cfg";
        case FILETYPE_STATE: return "sta";
        case FILETYPE_SCREENSHOT: return "snap";
        case FILETYPE_INI: return "ini";
        default: return "mame";
    }
}
static void make_path(char *out, size_t n, int type, const char *name)
{
    const char *base = base_for_type(type);
    if (!name || !*name) snprintf(out, n, "%s", base);
    else if (name[0] == '/') snprintf(out, n, "%s", name);
    else snprintf(out, n, "%s/%s", base, name);
}
static int hid_has(const uint8_t keys[6], uint8_t code)
{
    for (unsigned i=0;i<6;i++) if (keys[i]==code) return 1;
    return 0;
}
extern "C" void bm_input_set_keyboard(uint8_t modifiers, const uint8_t keys[6])
{
    uint32_t m=0;
    if(hid_has(keys,0x50)) m|=1u<<0;
    if(hid_has(keys,0x4f)) m|=1u<<1;
    if(hid_has(keys,0x52)) m|=1u<<2;
    if(hid_has(keys,0x51)) m|=1u<<3;
    if((modifiers & 0x11) || hid_has(keys,0x04)) m|=1u<<4;
    if(hid_has(keys,0x1e)) m|=1u<<5;
    if(hid_has(keys,0x1f)) m|=1u<<6;
    if(hid_has(keys,0x22)) m|=1u<<7;
    if(hid_has(keys,0x29)) m|=1u<<8;
    if(hid_has(keys,0x2c)) m|=1u<<9;
    if(hid_has(keys,0x2b)) m|=1u<<10;  /* Tab / MAME UI */
    if(hid_has(keys,0x28)) m|=1u<<11;  /* Enter */
    if(hid_has(keys,0x3b)) m|=1u<<12;  /* F2 Service */
    if(hid_has(keys,0x3c)) m|=1u<<13;  /* F3 Reset */
    if(hid_has(keys,0x13)) m|=1u<<14;  /* P Pause */
    if(modifiers & 0x22) m|=1u<<15;
    if(hid_has(keys,0x1d)) m|=1u<<16;  /* Z */
    if(hid_has(keys,0x1b)) m|=1u<<17;  /* X */
    if(hid_has(keys,0x06)) m|=1u<<18;  /* C */
    if(hid_has(keys,0x19)) m|=1u<<19;  /* V */
    if(hid_has(keys,0x05)) m|=1u<<31;  /* B: sixth arcade action button */
    if(hid_has(keys,0x1a)) m|=1u<<20;  /* W */
    if(hid_has(keys,0x04)) m|=1u<<21;  /* A */
    if(hid_has(keys,0x16)) m|=1u<<22;  /* S */
    if(hid_has(keys,0x07)) m|=1u<<23;  /* D */
    if(hid_has(keys,0x20)) m|=1u<<24;  /* 3 */
    if(hid_has(keys,0x21)) m|=1u<<25;  /* 4 */
    if(hid_has(keys,0x23)) m|=1u<<26;  /* 6 */
    if(hid_has(keys,0x24)) m|=1u<<27;  /* 7 */
    if(hid_has(keys,0x25)) m|=1u<<28;  /* 8 */
    if(hid_has(keys,0x26)) m|=1u<<29;  /* 9 */
    if(hid_has(keys,0x27)) m|=1u<<30;  /* 0 */
    __atomic_store_n(&g_keymask,m,__ATOMIC_RELEASE);
    __atomic_store_n(&g_altmask,
         (uint8_t)(((modifiers&0x04u)?1u:0u)|((modifiers&0x40u)?2u:0u)),
         __ATOMIC_RELEASE);
}
extern "C" void bm_input_set_gamepad(uint32_t mask)
{
    __atomic_store_n(&g_padmask,mask,__ATOMIC_RELEASE);
}

extern "C" int osd_init(void) { return 0; }
extern "C" void osd_wait_for_debugger(void) {}
extern "C" int osd_create_display(const osd_create_params *p, UINT32 *rgb)
{ if(rgb){rgb[0]=0xf800;rgb[1]=0x07e0;rgb[2]=0x001f;} (void)p; return 0; }
extern "C" void osd_close_display(void) {}
extern "C" int frf_skip_this_frame(void);
extern "C" void frf_skip_frame_finish(void);
extern "C" int osd_skip_this_frame(void) { return frf_skip_this_frame(); }
extern "C" void osd_update_boot_progress(int,int) {}
extern "C" void bm_system_poll(void);
extern "C" void bm_video_update_palette(const uint32_t *palette,uint32_t entries,unsigned dirty);
extern "C" void osd_update_video_and_audio(mame_display *d)
{
    bm_system_poll();
    if(!d || !d->game_bitmap || !d->game_bitmap->base) return;
    if(osd_skip_this_frame()){
        frf_skip_frame_finish();
        return;
    }
    mame_bitmap *b=d->game_bitmap;
    const uint32_t *pal=(const uint32_t *)(d->game_palette ? d->game_palette : game_palette);
    uint32_t pal_entries=d->game_palette_entries ? d->game_palette_entries : (uint32_t)palette_get_total_colors_with_ui();
    bm_video_update_palette(pal,pal_entries,(d->changed_flags & GAME_PALETTE_CHANGED)!=0);
    bm_submit_frame(b->base,(uint32_t)b->width,(uint32_t)b->height,(uint32_t)b->rowbytes,
                    (uint32_t)b->depth,pal,pal_entries);
    frf_skip_frame_finish();
}
extern "C" mame_bitmap *osd_override_snapshot(mame_bitmap *bitmap, rectangle *) { return bitmap; }
extern "C" const char *osd_get_fps_text(const performance_info *) { return ""; }

extern "C" int bm_audio_write(const INT16 *samples, unsigned frames);
#ifdef FRF_MAME_AUDIO_FAST
extern "C" void bm_audio_reset(void);
extern "C" unsigned bm_audio_source_rate(void);
#endif
static int g_audio_frames=800;
/* v0.53: fractional 48kHz samples per native arcade frame. */
static unsigned long long g_audio_phase=0, g_audio_rate_millihz=60000;
static int bm_next_audio_frames(void) { g_audio_phase+=
#ifdef FRF_MAME_AUDIO_FAST
(unsigned long long)bm_audio_source_rate()*1000ULL;
#else
48000000ULL;
#endif
    int n=(int)(g_audio_phase/g_audio_rate_millihz); g_audio_phase%=g_audio_rate_millihz; return n; }
/* Amiga display-only extras are unavailable in Circle bare metal.
   Zero disables optional OutRun steering-wheel overlay decorations. */
extern "C" unsigned int GetDisplayGoodiesFlags(void) { return 0u; }
extern "C" int osd_start_audio_stream(int stereo)
{
    (void)stereo;
    double hz=(Machine && Machine->refresh_rate>20.0f && Machine->refresh_rate<240.0f)? Machine->refresh_rate : 60.0;
    g_audio_rate_millihz=(unsigned long long)(hz*1000.0+0.5);
    frf_bm_native_refresh_millihz=(unsigned)g_audio_rate_millihz;
    g_audio_phase=0;
#ifdef FRF_MAME_AUDIO_FAST
    bm_audio_reset();
#endif
    g_audio_frames=bm_next_audio_frames();
    return g_audio_frames;
}
extern "C" int osd_update_audio_stream(INT16 *samples)
{
    if(samples && g_sound_enabled) bm_audio_write(samples,(unsigned)g_audio_frames);
    g_audio_frames=bm_next_audio_frames();
    return g_audio_frames;
}
extern "C" void osd_stop_audio_stream(void) {}
extern "C" void osd_set_mastervolume(int a) { g_master_volume=a; }
extern "C" int osd_get_mastervolume(void) { return g_master_volume; }
extern "C" void osd_sound_enable(int e) { g_sound_enabled=e; }
extern "C" int soundMixerIsOn(void) { return g_sound_enabled; }

extern "C" const os_code_info *osd_get_code_list(void) { return g_codes; }
extern "C" INT32 osd_get_code_value(os_code c)
{
    uint32_t k=__atomic_load_n(&g_keymask,__ATOMIC_ACQUIRE);
    uint32_t p=__atomic_load_n(&g_padmask,__ATOMIC_ACQUIRE);
    unsigned alt=__atomic_load_n(&g_altmask,__ATOMIC_ACQUIRE);
    switch(c) {
        case OSD_K_LEFT:return !!(k&(1u<<0)); case OSD_K_RIGHT:return !!(k&(1u<<1));
        case OSD_K_UP:return !!(k&(1u<<2)); case OSD_K_DOWN:return !!(k&(1u<<3));
        case OSD_K_CTRL:return !!(k&(1u<<4));
        case OSD_K_LALT:return !!(alt&1u);
        case OSD_K_RALT:return !!(alt&2u);
        case OSD_K_1:return !!(k&(1u<<5)) || !!(p&BM_PAD_START);
        case OSD_K_2:return !!(k&(1u<<6)); case OSD_K_5:return !!(k&(1u<<7)) || !!(p&BM_PAD_SELECT);
        case OSD_K_ESC:return !!(k&(1u<<8));
        case OSD_K_SPACE:return !!(k&(1u<<9));
        case OSD_K_TAB:return !!(k&(1u<<10));
        case OSD_K_ENTER:return !!(k&(1u<<11));
        case OSD_K_F2:return !!(k&(1u<<12));
        case OSD_K_F3:return !!(k&(1u<<13));
        case OSD_K_PAUSE:return !!(k&(1u<<14));
        case OSD_K_SHIFT:return !!(k&(1u<<15));
        case OSD_K_Z:return !!(k&(1u<<16));
        case OSD_K_X:return !!(k&(1u<<17));
        case OSD_K_C:return !!(k&(1u<<18));
        case OSD_K_V:return !!(k&(1u<<19));
        case OSD_K_B:return !!(k&(1u<<31));
        case OSD_K_W:return !!(k&(1u<<20));
        case OSD_K_A:return !!(k&(1u<<21));
        case OSD_K_S:return !!(k&(1u<<22));
        case OSD_K_D:return !!(k&(1u<<23));
        case OSD_K_3:return !!(k&(1u<<24));
        case OSD_K_4:return !!(k&(1u<<25));
        case OSD_K_6:return !!(k&(1u<<26));
        case OSD_K_7:return !!(k&(1u<<27));
        case OSD_K_8:return !!(k&(1u<<28));
        case OSD_K_9:return !!(k&(1u<<29));
        case OSD_K_0:return !!(k&(1u<<30));
        case OSD_J_LEFT:return !!(p&BM_PAD_LEFT) || !!(k&(1u<<21));
        case OSD_J_RIGHT:return !!(p&BM_PAD_RIGHT) || !!(k&(1u<<23));
        case OSD_J_UP:return !!(p&BM_PAD_UP) || !!(k&(1u<<20));
        case OSD_J_DOWN:return !!(p&BM_PAD_DOWN) || !!(k&(1u<<22));
        case OSD_J_B1:return !!(p&BM_PAD_B1) || !!(k&(1u<<4)) || !!(k&(1u<<9)); case OSD_J_B2:return !!(p&BM_PAD_B2) || !!(k&(1u<<16));
        case OSD_J_B3:return !!(p&BM_PAD_B3) || !!(k&(1u<<17));
        case OSD_J_B4:return !!(p&BM_PAD_B4) || !!(k&(1u<<18));
        case OSD_J_B5:return !!(p&BM_PAD_B5) || !!(k&(1u<<19));
        case OSD_J_B6:return !!(p&BM_PAD_B6) || !!(k&(1u<<31));
        case OSD_J_B7:return !!(p&BM_PAD_B7);
        case OSD_J_B8:return !!(p&BM_PAD_B8);
        case OSD_J_B9:return !!(p&BM_PAD_B9);
        case OSD_J_B10:return !!(p&BM_PAD_B10);
        case OSD_J_START:return !!(p&BM_PAD_START);
        case OSD_J_SELECT:return !!(p&BM_PAD_SELECT);
        default:return 0;
    }
}
extern "C" int osd_readkey_unicode(int) { return 0; }
extern "C" void osd_customize_inputport_list(input_port_default_entry *) {}
extern "C" int osd_joystick_needs_calibration(void) { return 0; }
extern "C" void osd_joystick_start_calibration(void) {}
extern "C" const char *osd_joystick_calibrate_next(void) { return 0; }
extern "C" void osd_joystick_calibrate(void) {}
extern "C" void osd_joystick_end_calibration(void) {}
extern "C" void osd_post_input_port_init_check(void) {}

extern "C" int osd_get_path_count(int) { return 1; }
extern "C" int osd_get_path_info(int type, int, const char *filename)
{
    char p[512]; struct stat st; make_path(p,sizeof p,type,filename);
    if(stat(p,&st)!=0) return PATH_NOT_FOUND;
    return S_ISDIR(st.st_mode) ? PATH_IS_DIRECTORY : PATH_IS_FILE;
}
extern "C" int osd_create_directory(int type, int, const char *dirname)
{
    char p[512]; make_path(p,sizeof p,type,dirname);
    if(mkdir(p,0777)==0 || errno==EEXIST) return 0;
    return 1;
}
extern "C" osd_file *osd_fopen(int type, int, const char *filename, const char *mode, osd_file_error *error)
{
    char p[512]; make_path(p,sizeof p,type,filename); FILE *f=fopen(p,mode);
    if(error) *error = f ? FILEERR_SUCCESS : FILEERR_NOT_FOUND;
    return reinterpret_cast<osd_file*>(f);
}
extern "C" int osd_fseek(osd_file *file, INT64 offset, int whence) { return fseek(reinterpret_cast<FILE*>(file),(long)offset,whence); }
extern "C" UINT64 osd_ftell(osd_file *file) { long p=ftell(reinterpret_cast<FILE*>(file)); return p<0?0:(UINT64)p; }
extern "C" int osd_feof(osd_file *file) { return feof(reinterpret_cast<FILE*>(file)); }
extern "C" UINT32 osd_fread(osd_file *file, void *buffer, UINT32 length) { return (UINT32)fread(buffer,1,length,reinterpret_cast<FILE*>(file)); }
extern "C" UINT32 osd_fwrite(osd_file *file, const void *buffer, UINT32 length) { return (UINT32)fwrite(buffer,1,length,reinterpret_cast<FILE*>(file)); }
extern "C" void osd_fclose(osd_file *file) { if(file) fclose(reinterpret_cast<FILE*>(file)); }

extern "C" cycles_t osd_cycles(void) { return (cycles_t)bm_cycles(); }
extern "C" cycles_t osd_cycles_per_second(void) { return (cycles_t)bm_cycles_per_second(); }
extern "C" cycles_t osd_profiling_ticks(void) { return (cycles_t)bm_cycles(); }
extern "C" void *osd_alloc_executable(size_t size) { return malloc(size); }
extern "C" void osd_free_executable(void *ptr) { free(ptr); }
extern "C" int osd_is_bad_read_ptr(const void *, size_t) { return 0; }

extern "C" volatile UINT32 frf74_core_updatescreen_calls=0;
extern "C" volatile UINT32 frf74_core_draw_screen_calls=0;
extern "C" volatile UINT32 frf74_core_skip_calls=0;
extern "C" volatile UINT32 frf74_core_update_video_calls=0;
extern "C" volatile cycles_t frf74_core_draw_screen_ticks=0;
extern "C" volatile cycles_t frf74_core_update_video_ticks=0;
