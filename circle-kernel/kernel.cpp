#include "kernel.h"
#include "frf_gui.h"
#include <circle/bcmframebuffer.h>
#include <circle/bcmpropertytags.h>
#include <circle/memory.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/startup.h>
#include <circle/devicenameservice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fatfs/ff.h>

extern "C" {
#include "../baremetal/bm_platform.h"
#include "../mame106/mame.h"
#ifndef INLINE
#define INLINE static inline
#endif
#include "../mame106/driver.h"
#include "../mame106/mamelog.h"
#include "../mame106/frf_perf_profile.h"
}

/* FRF v0.33 menu: raw HID navigation separate from MAME input.
   One key edge produces one action; callbacks cannot block or draw. */
extern "C" void bm_log_select_game(const char *game);
static CKernel *gUSBKernel=0;
static volatile unsigned gMenuEvent=0;
static volatile unsigned gRebootPending=0;
static unsigned gRebootComboHeld=0;
static uint32_t gUsbRescanCount=0,gUsbKbdReattach=0,gUsbPadReattach=0;
static uint32_t gUsbKeyboardReports=0,gUsbGamepadReports=0;
static uint32_t gUsbKbdRemove=0,gUsbPadRemove=0;
static uint64_t gUsbNextPollMs=0;
#ifdef FRF_MAME_AUDIO_FAST
extern "C" unsigned bm_audio_source_rate(void);
#endif

/* Safe poll from main scheduler/render thread; NEVER reboot from HID interrupt. */
extern "C" void bm_system_poll(void)
{
    /* Circle hot-plug is TASK_LEVEL only. Legacy builds never serviced it
       after Initialize(), causing permanent HID loss after disconnect. */
    if(gUSBKernel) gUSBKernel->ServiceUSBPlugAndPlay();
    if(__atomic_exchange_n(&gRebootPending,0,__ATOMIC_ACQ_REL)) {
        bm_log_mark("HOTKEY_CTRL_SHIFT_F11_REBOOT");
        reboot(); /* Circle firmware watchdog reboot, does not return */
    }
}

static char gLastPerfLine[168]={0};
static char gLastAudioLine[132]={0};
static char gLastCpuProfile[180]={0};
static char gLastFlipLine[128]={0};
static char gPerfExportStatus[96]={0};
extern "C" {
extern volatile uint64_t frf_bm_first_present_us, frf_bm_last_present_us;
extern volatile uint32_t frf_bm_max_present_gap_us;
extern volatile unsigned frf_bm_native_refresh_millihz;
extern volatile uint32_t frf_bm_flip_success, frf_bm_flip_failure, frf_bm_double_active;
extern volatile uint32_t frf_bm_render_us, frf_bm_vsync_us;
}
static unsigned gMenuHeld=0;
static bool gMenuActive=false;
static bool gVideoMenuActive=false;
/* 0 = request offset, fence with subsequent VBlank before page reuse.
   1 = legacy wait-before-offset (diagnostic comparison, may tear).
   2 = combined firmware tags (experimental). */
static unsigned gFlipMode=0;
static bool gFlipBatchSupported=true;
static const char *frf_flip_mode_name(void)
{
    switch(gFlipMode){
        case 0:return "QUEUED offset -> VBlank fence";
        case 1:return "LEGACY VBlank -> offset";
        default:return "BATCH VBlank+offset";
    }
}
static bool frf_menu_has_key(const unsigned char k[6],unsigned code)
{
    for(unsigned n=0;n<6;n++) if(k[n]==code) return true;
    return false;
}
static bool frf_rom_present(const char *name)
{
    char path[160];
    struct stat st;
    snprintf(path,sizeof(path),"roms/%s.zip",name);
    if(stat(path,&st)==0 && S_ISREG(st.st_mode)) return true;
    snprintf(path,sizeof(path),"roms/%s.ZIP",name);
    return stat(path,&st)==0 && S_ISREG(st.st_mode);
}
#include "frf_game_index.inc"
/* Lightweight, persistent run status. All games have a one-line result
   stored on SD, so failed-to-launch titles are not falsely marked playable. */
static unsigned char gGameResult[8192]={0};
static unsigned char gRomAudit[8192]={0};
static void frf_load_rom_audit(void){
    FIL f;
    if(f_open(&f,"SD:/FRFROMS.CSV",FA_READ)!=FR_OK)return;
    char line[160];
    while(f_gets(line,sizeof(line),&f)){
        char name[64];unsigned code=0;
        if(sscanf(line,"%63[^,],%u",name,&code)!=2||code>5u)continue;
        for(unsigned i=0;drivers[i]&&i<8192u;i++){
            if(strcmp(name,drivers[i]->name)==0){
                gRomAudit[i]=(unsigned char)code;break;
            }
        }
    }
    f_close(&f);
}
static const char *FRF_RUN_HISTORY="SD:/MAMERUN.CSV";
static void frf_load_results(void){
    FIL f;
    if(f_open(&f,FRF_RUN_HISTORY,FA_READ)!=FR_OK)return;
    char line[290];
    while(f_gets(line,sizeof(line),&f)){
        char name[64];int rc=0;
        unsigned srcw=0,srch=0,depth=0,fbw=0,fbh=0,oversize=0,flips=0;
        int got=sscanf(line,"%63[^,],%d,%u,%u,%u,%u,%u,%u,%u",
            name,&rc,&srcw,&srch,&depth,&fbw,&fbh,&oversize,&flips);
        if(got<2)continue;
        for(unsigned i=0;drivers[i]&&i<8192u;i++){
            if(strcmp(name,drivers[i]->name)==0){
                gGameResult[i]=rc?2u:(got>=9&&flips==0u?3u:1u);
                break;
            }
        }
    }
    f_close(&f);
}
extern "C" const char *bm_log_last_error(void);
extern "C" {
extern volatile uint32_t frf_bm_source_w,frf_bm_source_h,frf_bm_source_depth;
extern volatile uint32_t frf_bm_oversize_frames;
extern volatile uint32_t frf_bm_fb_width,frf_bm_fb_height;
}
static void frf_record_result(unsigned id,int rc){
    if(id>=8192u||!drivers[id])return;
    gGameResult[id]=rc?2u:(frf_bm_flip_success?1u:3u);
    FIL f;
    if(f_open(&f,FRF_RUN_HISTORY,FA_OPEN_ALWAYS|FA_WRITE)!=FR_OK)return;
    if(f_lseek(&f,f_size(&f))==FR_OK){
        char line[312];
        const char *msg=rc?bm_log_last_error():"OK";
        snprintf(line,sizeof(line),
          "%s,%d,%u,%u,%u,%u,%u,%u,%u,%u,%.92s\r\n",
          drivers[id]->name,rc,(unsigned)frf_bm_source_w,
          (unsigned)frf_bm_source_h,(unsigned)frf_bm_source_depth,
          (unsigned)frf_bm_fb_width,(unsigned)frf_bm_fb_height,
          (unsigned)frf_bm_oversize_frames,
          (unsigned)frf_bm_flip_success,(unsigned)frf_bm_flip_failure,
          msg&&*msg?msg:"No error text captured");
        UINT n=0;
        f_write(&f,line,(UINT)strlen(line),&n);
        f_sync(&f);
    }
    f_close(&f);
}

/* Source game timing shown before launch; cached per menu selection. */
/* v1.04 per-game frame skipping, saved by MAME driver short name. */
extern "C" void frf_skip_start(unsigned,unsigned);
extern "C" unsigned frf_skip_level(void);
extern "C" unsigned frf_skip_measured_hz(void);
extern "C" unsigned frf_skip_drawn(void);
extern "C" unsigned frf_skip_dropped(void);
/* The cached on-card index always contains ALL ROMs. A view is an
 * in-RAM projection: switching filters never scans or writes SD. */
static int frf_all_indices[8192];
static int frf_all_count=0;
static unsigned frf_view_mode=0; /* 0 all, 1 verified CRC, 2 Neo Geo */
static void frf_capture_all_games(void){
    frf_all_count=frf_game_count;
    for(int i=0;i<frf_all_count;i++)frf_all_indices[i]=frf_game_indices[i];
}
static void frf_apply_view(void){
    frf_game_count=0;
    for(int i=0;i<frf_all_count;i++){
        int id=frf_all_indices[i];
        bool show=(frf_view_mode==0) ||
            (frf_view_mode==1 && (gRomAudit[id]==1 || gRomAudit[id]==5)) ||
            (frf_view_mode==2 && drivers[id]->source_file &&
                strstr(drivers[id]->source_file,"neogeo.c")!=NULL);
        if(show)frf_game_indices[frf_game_count++]=id;
    }
}
static uint8_t gGameSkip[8192]={0}; /* 0 off, 1 auto, 2-5 manual skip 1-4 */
static const char *frf_skip_name(unsigned mode){
    switch(mode){
      case 1:return "AUTO (up to 3)";
      case 2:return "MANUAL 1";
      case 3:return "MANUAL 2";
      case 4:return "MANUAL 3";
      case 5:return "MANUAL 4";
      default:return "OFF";
    }
}
static void frf_load_skip_prefs(void){
    FIL f;char line[96];
    if(f_open(&f,"SD:/FRFSKIP.TXT",FA_READ)!=FR_OK)return;
    while(f_gets(line,sizeof(line),&f)){
        char name[48];unsigned mode=0;
        if(sscanf(line,"%47s %u",name,&mode)!=2 || mode>5u)continue;
        for(unsigned i=0;drivers[i] && i<8192u;i++){
            if(strcmp(drivers[i]->name,name)==0){gGameSkip[i]=(uint8_t)mode;break;}
        }
    }
    f_close(&f);
}
static bool frf_save_skip_prefs(void){
    FIL f;UINT written=0;
    if(f_open(&f,"SD:/FRFSKIP.TMP",FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)return false;
    bool ok=true;
    for(unsigned i=0;drivers[i] && i<8192u && ok;i++){
        if(!gGameSkip[i])continue;
        char line[96];
        unsigned n=(unsigned)snprintf(line,sizeof(line),"%s %u\n",
                                drivers[i]->name,(unsigned)gGameSkip[i]);
        written=0;
        ok=(f_write(&f,line,n,&written)==FR_OK && written==n);
    }
    if(ok)ok=f_sync(&f)==FR_OK;
    if(f_close(&f)!=FR_OK)ok=false;
    if(!ok){f_unlink("SD:/FRFSKIP.TMP");return false;}
    f_unlink("SD:/FRFSKIP.TXT");
    return f_rename("SD:/FRFSKIP.TMP","SD:/FRFSKIP.TXT")==FR_OK;
}
static int gGameTimingId=-1;
static machine_config gGameTimingCfg;
static unsigned gSelectedGameHzMillihz=0,gSelectedGameW=0,gSelectedGameH=0;
static void frf_update_game_timing(int id)
{
    if(id==gGameTimingId)return;
    gGameTimingId=id;
    gSelectedGameHzMillihz=gSelectedGameW=gSelectedGameH=0;
    if(id<0 || !drivers[id] || !drivers[id]->drv)return;
    expand_machine_driver(drivers[id]->drv,&gGameTimingCfg);
    if(gGameTimingCfg.frames_per_second>10.0f &&
       gGameTimingCfg.frames_per_second<240.0f)
        gSelectedGameHzMillihz=
            (unsigned)(gGameTimingCfg.frames_per_second*1000.0f+0.5f);
    gSelectedGameW=(unsigned)(gGameTimingCfg.screen_width>0?gGameTimingCfg.screen_width:0);
    gSelectedGameH=(unsigned)(gGameTimingCfg.screen_height>0?gGameTimingCfg.screen_height:0);
}
#ifdef FRF_TINYUSB_MAME_HOST
extern "C" void bm_tusb_stats(uint32_t out[7]);
extern "C" void bm_tusb_pad_detail(uint32_t out[12]);
extern "C" void bm_tusb_usb_device_info(uint32_t out[8]);
extern "C" void bm_tusb_hid_mount_info(uint32_t out[8]);
static uint32_t gPadLogVIDPID=0,gPadLogFlags=0;
static uint32_t gLastHidGen=0xffffffffu,gLastHidStage=0xffffffffu;
static void frf_log_hid_mount(const uint32_t d[8]){
    if(d[0]==gLastHidGen && d[7]==gLastHidStage)return;
    gLastHidGen=d[0];gLastHidStage=d[7];
    FIL f;
    if(f_open(&f,"SD:/USBPAD.TXT",FA_OPEN_ALWAYS|FA_WRITE)!=FR_OK)return;
    if(f_lseek(&f,f_size(&f))==FR_OK){
        char line[128];
        int n=snprintf(line,sizeof(line),
           "HID gen=%u id=%04X:%04X addr=%u inst=%u proto=%u desc=%u claim=%u switchStage=%u\r\n",
           (unsigned)d[0],(unsigned)(d[1]>>16),(unsigned)(d[1]&0xffffu),
           (unsigned)d[2],(unsigned)d[3],(unsigned)d[4],
           (unsigned)d[5],(unsigned)d[6],(unsigned)d[7]);
        if(n>0&&n<(int)sizeof(line)){
            UINT w=0;f_write(&f,line,(UINT)n,&w);f_sync(&f);
        }
    }
    f_close(&f);
}
static uint32_t gLastUsbDeviceGen=0xffffffffu;
static uint32_t gLastUsbHostState=0xffffffffu;
static void frf_log_usb_device(const uint32_t d[8],const uint32_t hw[7]){
    uint32_t host=(hw[0]&1u)|((hw[4]&255u)<<1)|((hw[6]&255u)<<9);
    if(d[3]==gLastUsbDeviceGen && host==gLastUsbHostState)return;
    gLastUsbDeviceGen=d[3];gLastUsbHostState=host;
    FIL file;
    if(f_open(&file,"SD:/USBPAD.TXT",FA_OPEN_ALWAYS|FA_WRITE)!=FR_OK)return;
    if(f_lseek(&file,f_size(&file))==FR_OK){
        char line[176];
        int n=snprintf(line,sizeof(line),
            "USB DEVICE gen=%u connected=%u candidate=%04X:%04X addr=%u "
            "lastVIDPID=%08X kbdAddr=%u padAddr=%u power=%u "
            "DWC2=%08X HPRT=%08X enumOK=%u enumFAIL=%u\r\n",
            (unsigned)d[3],(unsigned)d[2],(unsigned)(d[0]>>16),
            (unsigned)(d[0]&65535u),(unsigned)d[1],
            (unsigned)d[4],(unsigned)d[5],(unsigned)d[6],
            (unsigned)hw[0],(unsigned)hw[2],(unsigned)hw[3],
            (unsigned)hw[5],(unsigned)hw[6]);
        if(n>0 && n<(int)sizeof(line)){
            UINT w=0;f_write(&file,line,(UINT)n,&w);f_sync(&file);
        }
    }
    f_close(&file);
}
static void frf_log_usb_pad(const uint32_t d[12]){
    if(!d[0]){
        gPadLogVIDPID=0;gPadLogFlags=0;
        return;
    }
    /* Log once on mount, once when reports start, once on first nonzero
       button/axis sample. Never write while MAME is running. */
    uint32_t newflags=1u|(d[2]?2u:0u)|(d[3]?4u:0u);
    if(d[1]==gPadLogVIDPID && (newflags & ~gPadLogFlags)==0u)return;
    gPadLogVIDPID=d[1];gPadLogFlags|=newflags;
    FIL f;
    if(f_open(&f,"SD:/USBPAD.TXT",FA_OPEN_ALWAYS|FA_WRITE)!=FR_OK)return;
    if(f_lseek(&f,f_size(&f))==FR_OK){
        char line[208];
        int len=snprintf(line,sizeof(line),
         "PAD vid=%04X pid=%04X reports=%u mask=%04X reportid=%u len=%u "
         "decoder=%u raw=%08X,%08X Xbit=%u hatbit=%u usb_errors=%u\r\n",
         (unsigned)(d[1]>>16),(unsigned)(d[1]&0xffffu),
         (unsigned)d[2],(unsigned)(d[3]&0xffffu),
         (unsigned)d[4],(unsigned)d[5],(unsigned)d[8],
         (unsigned)d[6],(unsigned)d[7],(unsigned)d[10],
         (unsigned)d[11],(unsigned)d[9]);
        if(len>0 && (size_t)len<sizeof(line)){
            UINT wrote=0;
            (void)f_write(&f,line,(UINT)len,&wrote);
            (void)f_sync(&f);
        }
    }
    f_close(&f);
}
extern "C" void bm_tusb_hw_stats(uint32_t out[7]);
extern "C" void bm_log_usb_status(void);
#endif
static void frf_menu_draw(int selected)
{
    if(frf_gui_available()){
        frf_gui_model vm={};
        vm.count=(unsigned)frf_game_count;
        vm.total_count=(unsigned)frf_all_count;
        vm.filter_mode=frf_view_mode;
        vm.selected=(unsigned)(selected>=0?selected:0);
        int begin=selected-7;if(begin<0)begin=0;
        if(begin+FRF_GUI_ROWS>frf_game_count)
            begin=frf_game_count>FRF_GUI_ROWS?frf_game_count-FRF_GUI_ROWS:0;
        vm.first=(unsigned)begin;
        unsigned count=(unsigned)(frf_game_count-begin);
        vm.rows=count<FRF_GUI_ROWS?count:FRF_GUI_ROWS;
        for(unsigned i=0;i<vm.rows;i++){
            unsigned id=(unsigned)frf_game_indices[begin+(int)i];
            vm.items[i].title=drivers[id]->description;
            vm.items[i].short_name=drivers[id]->name;
            vm.items[i].status=gGameResult[id];
            vm.items[i].rom_status=gRomAudit[id];
        }
        if(frf_game_count && selected>=0 && selected<frf_game_count){
            unsigned id=(unsigned)frf_game_indices[selected];
            const game_driver *d=drivers[id];
            frf_update_game_timing((int)id);
            vm.selected_title=d->description;
            vm.selected_short=d->name;
            vm.selected_year=d->year;
            vm.selected_maker=d->manufacturer;
            vm.native_width=gSelectedGameW;
            vm.native_height=gSelectedGameH;
            vm.native_millihz=gSelectedGameHzMillihz;
            vm.frame_skip_mode=gGameSkip[id];
            vm.selected_status=gGameResult[id];
            vm.selected_rom_status=gRomAudit[id];
        }
        vm.double_buffer=(unsigned)bm_video_is_double();
        vm.last_performance=gLastPerfLine[0]?gLastPerfLine:0;
        vm.last_message=gPerfExportStatus[0]?gPerfExportStatus:0;
#ifdef FRF_TINYUSB_MAME_HOST
        uint32_t stats[7]={0};bm_tusb_stats(stats);
        vm.usb_keyboard=stats[1];vm.usb_gamepad=stats[2];
        uint32_t pad[12]={0};bm_tusb_pad_detail(pad);
        vm.usb_pad_connected=pad[0];vm.usb_pad_vidpid=pad[1];
        vm.usb_pad_reports=pad[2];vm.usb_pad_mask=pad[3];
        vm.usb_pad_report_id=pad[4];vm.usb_pad_decoder=pad[8];
        uint32_t host[7]={0};bm_tusb_hw_stats(host);
        vm.usb_host_ok=host[0];vm.usb_device_count=host[4];
        vm.usb_enum_errors=host[6];
        uint32_t anyusb[8]={0};bm_tusb_usb_device_info(anyusb);
        vm.usb_any_vidpid=anyusb[0];
        vm.usb_any_addr=anyusb[1];
#endif
        log_enableStdOut(0); /* prevent console messages overwriting GUI */
        frf_gui_render(&vm);
        return;
    }
    printf("\033[2J\033[H");
    printf("  FUTURE RETRO FUSION - MAME106RGB BARE METAL\n");
    printf("  ========================================\n");
#ifdef FRF_TINYUSB_MAME_HOST
    printf("  Pi3 TinyUSB DWC2 - USB HID keyboard and gamepad\n");
#ifdef FRF_BM_ASYNC_RENDER
    printf("  Pipeline: CPU0 MAME + CPU1 USB + CPU2 video/VBlank\n\n");
#else
    printf("  Pipeline: CPU0 MAME/video + CPU1 USB (CPU2/3 parked)\n\n");
#endif
#else
    printf("  Pi3 Circle USB PnP - keyboard/gamepad\n\n");
#endif
    if(!frf_game_count) {
        printf("  No ROM ZIPs found in /roms\n");
        printf("  Add MAME 0.106 compatible ROMs and reboot.\n");
    } else {
        int begin=selected-7;
        if(begin<0) begin=0;
        for(int row=0;row<15 && begin+row<frf_game_count;row++) {
            int n=begin+row;
            const game_driver *d=drivers[frf_game_indices[n]];
            printf("  %c %-14s %s\n",selected==n?'>':' ',d->name,d->description);
        }
        printf("\n  %d games available, showing %d-%d\n",frf_game_count,begin+1,
               begin+15<frf_game_count?begin+15:frf_game_count);
    }
    if(frf_game_count && selected>=0 && selected<frf_game_count) {
        frf_update_game_timing(frf_game_indices[selected]);
        if(gSelectedGameHzMillihz)
            printf("\n  Native: %ux%u  %u.%03u Hz  (game timing)\n",
                   gSelectedGameW,gSelectedGameH,
                   gSelectedGameHzMillihz/1000u,
                   gSelectedGameHzMillihz%1000u);
    }
    if(frf_game_count && selected>=0 && selected<frf_game_count)
        printf("  Frameskip for %s: %s [K changes, saved to SD]\n",
               drivers[frf_game_indices[selected]]->name,
               frf_skip_name(gGameSkip[frf_game_indices[selected]]));
    printf("  Index: %s (R rebuild after copying ROMs)\n",frf_index_from_cache?"CACHED":"REBUILT");
    printf("  TAB / Gamepad B4: ALL / ROM VERIFIED / NEOGEO (%u)\n",frf_view_mode);
    printf("  UP/DOWN select  PAGE UP/DOWN jump  ENTER start  R rescan\n");
    printf("  K frameskip: OFF/AUTO/1/2/3/4 (per game, persistent)\n");
    printf("  In game: 5 coin  1 start  ARROWS/WASD move  Space/Z jump\n");
    printf("  V HDMI modes  T scrolling tear test  P export last test\n");
    printf("  CTRL+SHIFT+F11 reboot\n");
#ifdef FRF_VSYNC_BATCH_TAGS
    printf("  B flip mode: %s%s\n",frf_flip_mode_name(),
           gFlipBatchSupported?"":" [batch unsupported]");
#endif
#ifdef FRF_MAME_AUDIO_FAST
    printf("  Audio: %u kHz mix / 48 kHz HDMI [F toggle]\n",bm_audio_source_rate()/1000u);
#endif
    if(gLastPerfLine[0]) printf("\n  %s\n",gLastPerfLine);
    if(gLastAudioLine[0])printf("  %s\n",gLastAudioLine);
    if(gLastCpuProfile[0])printf("  %s\n",gLastCpuProfile);
    if(gLastFlipLine[0])printf("  %s\n",gLastFlipLine);
    if(gPerfExportStatus[0])printf("  %s\n",gPerfExportStatus);
#ifdef FRF_TINYUSB_MAME_HOST
    uint32_t usb[7]={0};bm_tusb_stats(usb);
    uint32_t hw[7]={0};bm_tusb_hw_stats(hw);
    printf("  TinyUSB: host=%u keyboard=%u gamepad=%u key reports=%u pad reports=%u\n",
           (unsigned)usb[0],(unsigned)usb[1],(unsigned)usb[2],
           (unsigned)usb[3],(unsigned)usb[4]);
    printf("  USB power=%u state=%u DWC2 ID=%08x root=%08x dev=%u enumOK=%u fail=%u\n",
           (unsigned)hw[0],(unsigned)hw[1],(unsigned)hw[2],
           (unsigned)hw[3],(unsigned)hw[4],(unsigned)hw[5],(unsigned)hw[6]);
#endif
    fflush(stdout);
}
static CBcmFrameBuffer *gVideoFrameBuffer=0;
#include "frf_videocore_edid.inc"
#ifdef FRF_VCHI_MODE_QUERY
extern "C" unsigned frf_vc4_enumerate(CInterruptSystem *irq,void *dest,unsigned cap,void *state);
#endif
extern "C" int bm_display_flip(unsigned page)
{
    if(!gVideoFrameBuffer || gVideoFrameBuffer->GetVirtHeight() <
         2u*gVideoFrameBuffer->GetHeight()) return 0;
    /* v0.84: mailbox offset is queued for the NEXT VBlank. The
       safe ordering is SET OFFSET then WAIT FOR THAT VBLANK, so
       the former front buffer cannot be reused before scanout switches. */
    if(gFlipMode==0u){
        if(!gVideoFrameBuffer->SetVirtualOffset(
              0,page*gVideoFrameBuffer->GetHeight()))return 0;
        return gVideoFrameBuffer->WaitForVerticalSync()?1:0;
    }
    /* v0.71: A VSync wait followed by a separate mailbox request can
       miss the vertical blank and visibly tear ~100 lines down. Submit
       both firmware property tags in ONE mailbox transaction, ordered
       WAIT_FOR_VSYNC -> SET_VIRTUAL_OFFSET. Firmware ordering must be
       verified on real Pi hardware; fall back to legacy on failure. */
#ifdef FRF_VSYNC_BATCH_TAGS
    struct {
        TPropertyTagSimple vsync;
        TPropertyTagVirtualOffset offset;
    } tagsReq={};
    tagsReq.vsync.Tag.nTagId=PROPTAG_WAIT_FOR_VSYNC;
    tagsReq.vsync.Tag.nValueBufSize=4u;
    tagsReq.vsync.Tag.nValueLength=0u;
    tagsReq.offset.Tag.nTagId=PROPTAG_SET_VIRTUAL_OFFSET;
    tagsReq.offset.Tag.nValueBufSize=8u;
    tagsReq.offset.Tag.nValueLength=8u;
    tagsReq.offset.nOffsetX=0u;
    tagsReq.offset.nOffsetY=page*gVideoFrameBuffer->GetHeight();
    CBcmPropertyTags props;
    if(gFlipMode==2u && gFlipBatchSupported &&
       props.GetTags(&tagsReq,sizeof(tagsReq)) &&
       (tagsReq.vsync.Tag.nValueLength&VALUE_LENGTH_RESPONSE) &&
       (tagsReq.offset.Tag.nValueLength&VALUE_LENGTH_RESPONSE) &&
       tagsReq.offset.nOffsetX==0u &&
       tagsReq.offset.nOffsetY==page*gVideoFrameBuffer->GetHeight())
        return 1;
    if(gFlipMode==2u && gFlipBatchSupported){
        gFlipBatchSupported=false;
        /* Avoid falling back to a method that reuses the front buffer. */
        if(!gVideoFrameBuffer->SetVirtualOffset(
             0,page*gVideoFrameBuffer->GetHeight()))return 0;
        return gVideoFrameBuffer->WaitForVerticalSync()?1:0;
    }
#endif
    if(!gVideoFrameBuffer->WaitForVerticalSync()) return 0;
    return gVideoFrameBuffer->SetVirtualOffset(0,page*gVideoFrameBuffer->GetHeight())?1:0;
}
/* A standalone presentation test, with NO emulated arcade CPU.
   Smooth 1 px/frame horizontal movement reveals horizontal tear boundaries. */
extern "C" void bm_video_update_palette(const uint32_t*,uint32_t,unsigned);
static uint16_t gTearPattern[320u*240u] __attribute__((aligned(64)));
static void frf_run_tear_test(void)
{
    static const uint32_t palette[4]={
        0x00101018u,0x00f0f0f0u,0x002a4478u,0x00f0ca39u
    };
    gMenuActive=false;
    log_enableStdOut(0);
    bm_video_begin_game();
    bm_video_set_orientation(0);
    bm_video_update_palette(palette,4,1);
    for(unsigned f=0;f<240u;f++){
        for(unsigned y=0;y<240u;y++){
            uint16_t *row=&gTearPattern[y*320u];
            for(unsigned x=0;x<320u;x++){
                unsigned v=((x+f)&63u)<32u?1u:2u;
                if(y==119u || y==120u || ((x+(f>>1))%160u)<2u)v=3u;
                row[x]=(uint16_t)v;
            }
        }
        bm_submit_frame(gTearPattern,320,240,640,16,palette,4);
        bm_system_poll();
    }
    bm_video_end_game();
    log_enableStdOut(1);
    uint64_t elapsed=frf_bm_last_present_us>frf_bm_first_present_us?
                     frf_bm_last_present_us-frf_bm_first_present_us:0u;
    uint32_t flips=frf_bm_flip_success;
    unsigned fps100=elapsed && flips>1u?
          (unsigned)(((uint64_t)(flips-1u)*100000000ull)/elapsed):0u;
    snprintf(gLastPerfLine,sizeof(gLastPerfLine),
             "TEAR TEST: %u.%02u FPS, %u flips, max gap %uus",
             fps100/100u,fps100%100u,(unsigned)flips,
             (unsigned)frf_bm_max_present_gap_us);
    snprintf(gLastFlipLine,sizeof(gLastFlipLine),
             "Video: double=%u, flip OK=%u, failed=%u, method=%s",
             (unsigned)frf_bm_double_active,
             (unsigned)frf_bm_flip_success,
             (unsigned)frf_bm_flip_failure,frf_flip_mode_name());
    gMenuHeld=0;
    gMenuActive=true;
}
static CHDMISoundBaseDevice *gAudioDevice = 0;
/* v0.86: diagnose audio queue pressure without SD logging.
   HDMI Write() is non-blocking and may accept fewer bytes than requested. */
static uint32_t gAudioAcceptedFrames=0,gAudioDroppedFrames=0;
static uint32_t gAudioPeakQueuedFrames=0,gAudioWriteCalls=0;
static int frf_audio_queue_write(const short *samples,unsigned frames)
{
    if(!gAudioDevice || !gAudioDevice->IsActive() || !samples)return 0;
    int bytes=gAudioDevice->Write(samples,(size_t)frames*4u);
    unsigned accepted=bytes>0?(unsigned)bytes/4u:0u;
    if(accepted>frames)accepted=frames;
    gAudioAcceptedFrames+=accepted;
    gAudioDroppedFrames+=frames-accepted;
    if((++gAudioWriteCalls&15u)==0u){
        unsigned q=gAudioDevice->GetQueueFramesAvail();
        if(q>gAudioPeakQueuedFrames)gAudioPeakQueuedFrames=q;
    }
    return bytes;
}
static void frf_audio_begin_game(void)
{
    gAudioAcceptedFrames=gAudioDroppedFrames=0;
    gAudioPeakQueuedFrames=gAudioWriteCalls=0;
    if(gAudioDevice)gAudioDevice->Flush();
}
#ifdef FRF_MAME_AUDIO_FAST
static bool gFastAudioMix=true;
extern "C" unsigned bm_audio_source_rate(void){return gFastAudioMix?32000u:48000u;}
#endif
#ifdef FRF_MAME_AUDIO_FAST
#include "frf_audio_fast.inc"
#endif
extern "C" int bm_audio_write(const short *samples, unsigned frames)
{
    if(!gAudioDevice || !gAudioDevice->IsActive() || !samples) return 0;
    /* MAME's finalmix is signed 16-bit interleaved stereo. */
#ifdef FRF_MAME_AUDIO_FAST
    if(gFastAudioMix)return frf_audio_resample_write(samples,frames);
    return frf_audio_queue_write(samples,frames);
#else
    return frf_audio_queue_write(samples,frames);
#endif
}

/* On-demand single write, not continuous gameplay logging. */
static bool frf_export_last_perf(void)
{
    FIL file;
    if(f_open(&file,"SD:/VIDPERF.TXT",FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)
        return false;
    bool ok=true;
    char buf[1024];
    snprintf(buf,sizeof(buf),
       "MAME106RGB last test performance\n%s\n%s\n%s\n"
       "Native game refresh=%u.%03uHz\n"
       "Flip method=%s\n"
       "Double buffer=%u, flip successes=%u, failures=%u\n"
       "Render_us_sum=%u VSync_us_sum=%u max_frame_gap_us=%u\n"
       "HDMI audio accepted=%u dropped=%u peak_queue=%u mixer_hz=%u\n"
       "MAME frame breakdown: %s\n"
       "Frameskip: %s\n",
       gLastPerfLine,gLastFlipLine,gLastAudioLine,
       (unsigned)frf_bm_native_refresh_millihz/1000u,
       (unsigned)frf_bm_native_refresh_millihz%1000u,
       frf_flip_mode_name(),(unsigned)frf_bm_double_active,
       (unsigned)frf_bm_flip_success,(unsigned)frf_bm_flip_failure,
       (unsigned)frf_bm_render_us,(unsigned)frf_bm_vsync_us,
       (unsigned)frf_bm_max_present_gap_us,
       (unsigned)gAudioAcceptedFrames,(unsigned)gAudioDroppedFrames,
       (unsigned)gAudioPeakQueuedFrames,(unsigned)options.samplerate,
       gLastCpuProfile,gPerfExportStatus);
    UINT n=0;
    unsigned len=(unsigned)strlen(buf);
    ok=(f_write(&file,buf,len,&n)==FR_OK && n==len);
    if(ok)ok=(f_sync(&file)==FR_OK);
    if(f_close(&file)!=FR_OK)ok=false;
    return ok;
}
CKernel *CKernel::sThis = 0;

CKernel::CKernel(void)
: CStdlibAppStdio("Mame106RGB-BareMetal"),
  mWorkers(CMemorySystem::Get()),
  mKeyboard(0),
  mGamePad(0)
{
    sThis=this;
    gUSBKernel=this;
}

bool CKernel::Initialize(void)
{
#ifdef FRF_TINYUSB_MAME_HOST
    /* Circle USB host must not acquire Pi3 DWC2. Initialize only the
       non-USB parts of CStdlibAppStdio (SD FATFS, video, console, logger). */
    if(!CStdlibAppScreen::Initialize()) return false;
    if(!mEMMC.Initialize()) return false;
    if(f_mount(&mFileSystem,"SD:",1)!=FR_OK) return false;
    if(!mConsole.Initialize()) return false;
    CGlueStdioInit(mConsole);
#else
    if (!CStdlibAppStdio::Initialize())
        return false;
#endif

    CBcmFrameBuffer *fb = mScreen.GetFrameBuffer();
    if (!fb)
        return false;

    bm_video_attach((void *)(uintptr_t)fb->GetBuffer(),
                    fb->GetWidth(),
                    fb->GetHeight(),
                    fb->GetPitch());
    frf_gui_attach((void *)(uintptr_t)fb->GetBuffer(),
                   fb->GetWidth(),fb->GetHeight(),fb->GetPitch(),16u);
    gVideoFrameBuffer=fb;
    bm_video_configure_double(fb->GetVirtHeight(),fb->GetSize(),bm_display_flip);
    frf_gui_configure_double(fb->GetVirtHeight(),fb->GetSize(),bm_display_flip);

    bm_video_set_scanlines(64); /* FRF v0.51: PiStormAGA-equivalent ~25% */
    bm_workers_init();

#ifndef FRF_TINYUSB_MAME_HOST
    AttachUSBInput();
#endif
    return mWorkers.Initialize();
}

void CKernel::AttachUSBInput(void)
{
    /* Rebind only after PnP service has removed dead devices; never register
       the same callback twice on a still-attached Circle HID object. */
    if(!mKeyboard) {
        auto *kb=(CUSBKeyboardDevice *)mDeviceNameService.GetDevice("ukbd1",FALSE);
        if(kb) {
            mKeyboard=kb;
            kb->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw);
            kb->RegisterRemovedHandler(KeyboardRemovedHandler,this);
            ++gUsbKbdReattach;
        }
    }
    if(!mGamePad) {
        auto *pad=(CUSBGamePadDevice *)mDeviceNameService.GetDevice("upad",1,FALSE);
        if(pad) {
            mGamePad=pad;
            pad->RegisterStatusHandler(GamePadStatusHandler);
            pad->RegisterRemovedHandler(GamePadRemovedHandler,this);
            const TGamePadState *st=pad->GetInitialState();
            if(st) GamePadStatusHandler(0,st);
            ++gUsbPadReattach;
        }
    }
}
void CKernel::ServiceUSBPlugAndPlay(void)
{
    /* Poll every 500ms; NEVER in USB HID callback. Enabled in both launcher
       and game loops (via bm_system_poll). */
    const uint64_t now=CTimer::GetClockTicks64()/1000u;
    if(now<gUsbNextPollMs) return;
    gUsbNextPollMs=now+500u;
#ifndef FRF_TINYUSB_MAME_HOST
    ++gUsbRescanCount;
    mUSBHCI.UpdatePlugAndPlay();
    AttachUSBInput();
#endif
}
#ifdef FRF_TINYUSB_MAME_HOST
extern "C" void bm_tusb_stats(uint32_t out[7]);
extern "C" void bm_tusb_kbd_input(uint8_t modifier,const uint8_t keys[6])
{
    CKernel::KeyStatusHandlerRaw(modifier,keys);
}
extern "C" void bm_tusb_pad_input(uint32_t mask)
{
    bm_input_set_gamepad(mask);
    static uint32_t held=0;
    if(gMenuActive) {
        uint32_t k=0;
        if(mask & BM_PAD_UP) k=1;
        else if(mask & BM_PAD_DOWN) k=2;
        else if(mask & (BM_PAD_START|BM_PAD_B1)) k=3;
        else if(mask & BM_PAD_B4) k=16;
        else if(mask & BM_PAD_SELECT) k=4;
        if(k && k!=held)
            __atomic_store_n(&gMenuEvent,k,__ATOMIC_RELEASE);
        held=k;
    } else held=0;
}
#endif
extern "C" void bm_usb_stats(uint32_t out[7])
{
    if(!out)return;
#ifdef FRF_TINYUSB_MAME_HOST
    bm_tusb_stats(out);
#else
    out[0]=gUsbRescanCount;
    out[1]=gUsbKbdReattach;
    out[2]=gUsbPadReattach;
    out[3]=gUsbKeyboardReports;
    out[4]=gUsbGamepadReports;
    out[5]=gUsbKbdRemove;
    out[6]=gUsbPadRemove;
#endif
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
    mLogger.Write(GetKernelName(), LogNotice,
#ifdef FRF_TINYUSB_MAME_HOST
                  "Mame106RGB TinyUSB: CPU0 MAME, CPU1 TinyUSB DWC2 exclusive host");
#else
                  "Mame106RGB bare metal: CPU0 MAME / CPU1 parked / CPU2 parked");
#endif
    mLogger.Write(GetKernelName(), LogNotice,
                  "Controls: arrows move, Ctrl/A or Space jump, 5 coin, 1 start");

    bm_log_init();
    mLogger.Write(GetKernelName(),bm_log_is_ready()?LogNotice:LogError,
                  "SD diagnostic logger %s: SD:/MAMELOG.TXT",bm_log_is_ready()?"READY":"FAILED");
    bm_log_mark(bm_video_is_double()?"VIDEO_DOUBLE_BUFFER_ON":"VIDEO_DOUBLE_BUFFER_UNAVAILABLE");
#ifdef FRF_TINYUSB_MAME_HOST
    bm_log_mark("USB_BACKEND_TINYUSB_EXCLUSIVE_DWC2");
#else
    bm_log_mark("USB_BACKEND_CIRCLE_PNP");
#endif
    log_enableStdOut(1);
    log_addCallback(bm_log_error_callback);
    options.skip_disclaimer = 1;
    options.skip_gameinfo = 1;
    /* HDMI stereo PCM, 48kHz matches 800 samples per 60Hz MAME frame. */
    gAudioDevice = new CHDMISoundBaseDevice(&mInterrupt, 48000, 384*10);
    if(gAudioDevice && gAudioDevice->AllocateQueue(100)) {
        gAudioDevice->SetWriteFormat(SoundFormatSigned16,2);
        if(!gAudioDevice->Start()) {
            mLogger.Write(GetKernelName(),LogWarning,"HDMI sound start failed");
            gAudioDevice = 0;
        }
    } else {
        mLogger.Write(GetKernelName(),LogWarning,"HDMI sound queue failed");
        gAudioDevice = 0;
    }
    options.samplerate =
#ifdef FRF_MAME_AUDIO_FAST
        (int)bm_audio_source_rate();
#else
        48000;
#endif
    options.use_samples = 1;

    frf_query_hdmi_modes();
#ifdef FRF_VCHI_MODE_QUERY
    gVC4ModeCount=frf_vc4_enumerate(&mInterrupt,gVC4Modes,256u,&gVC4State);
    gVC4ModesReady=gVC4ModeCount>0u;
#endif
    frf_init_games();
    frf_load_skip_prefs();
    frf_load_results();
    frf_load_rom_audit();
    frf_capture_all_games();
    frf_apply_view();
    int selected=0, lastSelected=-1;
    gMenuActive=true;
    frf_menu_draw(selected);
#ifdef FRF_TINYUSB_MAME_HOST
    uint32_t lastUsbDisplay=0xffffffffu;
    uint32_t lastUsbReports=0u;
    uint32_t lastUsbGeneration=0xffffffffu;
    uint32_t lastPadButtons=0xffffffffu,lastPadReportId=0xffffffffu;
    uint64_t nextUsbCheck=0;
#endif
    for(;;) {
        bm_system_poll();
#ifdef FRF_TINYUSB_MAME_HOST
        uint64_t nowUsb=CTimer::GetClockTicks64()/1000u;
        if(nowUsb>=nextUsbCheck) {
            nextUsbCheck=nowUsb+500u;
            uint32_t stats[7]={0};bm_tusb_stats(stats);
            uint32_t hw[7]={0};bm_tusb_hw_stats(hw);
            uint32_t status=(stats[0]?1u:0u) |
                            ((stats[1]&255u)<<1) |
                            ((stats[2]&255u)<<9) |
                            ((hw[0]&1u)<<17) |
                            ((hw[4]&255u)<<18);
            uint32_t pad[12]={0};bm_tusb_pad_detail(pad);
            uint32_t usbdev[8]={0};bm_tusb_usb_device_info(usbdev);
            frf_log_usb_device(usbdev,hw);
            uint32_t hid[8]={0};bm_tusb_hid_mount_info(hid);
            frf_log_hid_mount(hid);
            frf_log_usb_pad(pad);
            if(status!=lastUsbDisplay ||
               usbdev[3]!=lastUsbGeneration ||
               (stats[4]>0 && !lastUsbReports) ||
               (pad[0] && (pad[3]!=lastPadButtons ||
                pad[4]!=lastPadReportId))){
                lastUsbDisplay=status;
                lastUsbGeneration=usbdev[3];
                lastUsbReports=stats[4];
                lastPadButtons=pad[3];
                lastPadReportId=pad[4];
                bm_log_usb_status();
                lastSelected=-1; /* redraw detected pad state */
            }
        }
#endif
        unsigned event=__atomic_exchange_n(&gMenuEvent,0,__ATOMIC_ACQ_REL);
        if(gVideoMenuActive) {
            if(event==8 || event==9){
                gVideoMenuActive=false;
                frf_gui_after_game(); /* restore GUI's known page-0 baseline */
                lastSelected=-1;
                frf_menu_draw(selected);
                lastSelected=selected;
            } else if(event==12){
                gVideoExportStatus=frf_export_video_modes()?
                    "Export complete: SD:/VIDMODES.TXT":
                    "Export failed: check SD FAT filesystem";
                frf_show_hdmi_modes();
            } else if(event==5 && gVideoModePage>0){
                --gVideoModePage;frf_show_hdmi_modes();
            } else if(event==6 && (unsigned)(gVideoModePage+1)*14u<(gVC4ModeCount?gVC4ModeCount:gVideoModeCount)){
                ++gVideoModePage;frf_show_hdmi_modes();
            }
            mTimer.MsDelay(20);
            continue;
        }
        if(event==16){
            frf_view_mode=(frf_view_mode+1u)%3u;
            frf_apply_view();
            selected=0;
            lastSelected=-1;
        }
        if((event==14 || event==15) && frf_game_count){
            int id=frf_game_indices[selected];
            gGameSkip[id]=(uint8_t)((gGameSkip[id]+(event==14?1u:5u))%6u);
            if(!frf_save_skip_prefs())
                snprintf(gPerfExportStatus,sizeof(gPerfExportStatus),
                         "Frameskip preference save FAILED");
            lastSelected=-1;
        }
        if(event==10) {gFlipMode=(gFlipMode+1u)%3u;lastSelected=-1;}
        if(event==13){
            bool ok=frf_export_last_perf();
            snprintf(gPerfExportStatus,sizeof(gPerfExportStatus),
                     "%s",ok?"Last test saved: SD:/VIDPERF.TXT":
                             "Could not save VIDPERF.TXT (SD write failed)");
            lastSelected=-1;
        }
        if(event==11) {
            frf_gui_before_game();
            frf_run_tear_test();
            frf_gui_after_game();
            lastSelected=-1;
        }
        if(event==8){
            frf_gui_before_game(); /* video diagnostics draw console page 0 */
            log_enableStdOut(1);
            gVideoMenuActive=true;
            gVideoModePage=0;
            frf_measure_hdmi_vsync();
            frf_show_hdmi_modes();
            mTimer.MsDelay(20);
            continue;
        }
#ifdef FRF_MAME_AUDIO_FAST
        if(event==7) {gFastAudioMix=!gFastAudioMix; lastSelected=-1;}
#endif
        /* Fast alphabetical browsing; repeated initial jumps to next match.
           This operates only on the existing sorted in-RAM game index. */
        if(event>=100u && event<126u && frf_game_count){
            const char initial=(char)('A'+(event-100u));
            for(int delta=1;delta<=frf_game_count;delta++){
                int i=(selected+delta)%frf_game_count;
                const char *title=drivers[frf_game_indices[i]]->description;
                while(*title==' ' || *title=='(')title++;
                char first=*title;
                if(first>='a'&&first<='z')first=(char)(first-32);
                if(first==initial){selected=i;break;}
            }
        }
        if(event==1 && selected>0) --selected;
        if(event==2 && selected+1<frf_game_count) ++selected;
        if(event==5) selected=selected>=FRF_GUI_ROWS?selected-FRF_GUI_ROWS:0;
        if(event==6) selected+FRF_GUI_ROWS<frf_game_count?selected+=FRF_GUI_ROWS:selected=frf_game_count?frf_game_count-1:0;
        if(event==4) {
            frf_refresh_games();
            frf_capture_all_games();
            frf_apply_view();
            if(selected>=frf_game_count)selected=frf_game_count?frf_game_count-1:0;
            lastSelected=-1;
        }
        if(selected!=lastSelected) {
            lastSelected=selected;
            frf_menu_draw(selected);
        }
        if(event==3 && frf_game_count) {
            int id=frf_game_indices[selected];
            gMenuActive=false;
            frf_gui_before_game();
            frf_bm_native_refresh_millihz=0;
            bm_video_begin_game();
            frf_update_game_timing(id);
            frf_skip_start(gGameSkip[id],gSelectedGameHzMillihz);
            /* The proven Donkey Kong rotation is applied only for
               portrait/rotated MAME titles. Landscape stays native. */
            bm_video_set_orientation(drivers[id]->flags & ORIENTATION_MASK);
            /* New per-game log keeps Bomb Jack's startup failure and
               Time Pilot's timing visible, even after earlier launches. */
            bm_log_select_game(drivers[id]->name);
            bm_log_init();
            bm_log_mark(bm_video_is_double()?"VIDEO_DOUBLE_BUFFER_ON":"VIDEO_DOUBLE_BUFFER_UNAVAILABLE");
            char gameTag[64];
            snprintf(gameTag,sizeof gameTag,"GAME_%s",drivers[id]->name);
            bm_log_mark(gameTag);
            printf("\033[2J\033[HLaunching %s\n",drivers[id]->description);
            fflush(stdout);
            /* Console text writes to framebuffer page 0 and can corrupt a
               game frame while page 0 is visible. MAME messages continue
               into the SD logging callback during gameplay. */
            log_enableStdOut(0);
#ifdef FRF_MAME_AUDIO_FAST
            options.samplerate=(int)bm_audio_source_rate();
#endif
            frf_audio_begin_game();
            const int rc=run_game(id);
            bm_video_end_game();
            frf_gui_after_game();
            /* v0.60: retain stats in RAM, never write to the SD log.
               Count actual successful framebuffer flips, not draw calls. */
            uint32_t flips=frf_bm_flip_success;
            uint64_t t0=frf_bm_first_present_us, t1=frf_bm_last_present_us;
            if(flips>1 && t1>t0) {
                unsigned fpsx100=(unsigned)(((uint64_t)(flips-1)*100000000ull)/(t1-t0));
                unsigned ravg=frf_bm_render_us/flips;
                unsigned vavg=frf_bm_vsync_us/flips;
                snprintf(gLastPerfLine,sizeof gLastPerfLine,
                    "%s: native %u.%03uHz, present %u.%02u FPS, render %uus, vsync %uus, gap %uus",
                    drivers[id]->name,frf_bm_native_refresh_millihz/1000u,
                    frf_bm_native_refresh_millihz%1000u,fpsx100/100u,fpsx100%100u,
                    ravg,vavg,(unsigned)frf_bm_max_present_gap_us);
            } else {
                snprintf(gLastPerfLine,sizeof gLastPerfLine,
                    "%s: no verified double-buffer frame rate",drivers[id]->name);
            }
            snprintf(gLastAudioLine,sizeof(gLastAudioLine),
                     "HDMI audio: accepted=%u frames, dropped=%u, peak queue=%u, mixer=%uHz",
                     (unsigned)gAudioAcceptedFrames,(unsigned)gAudioDroppedFrames,
                     (unsigned)gAudioPeakQueuedFrames,(unsigned)options.samplerate);
            snprintf(gLastFlipLine,sizeof(gLastFlipLine),
                     "Video: double=%u, flip OK=%u, failed=%u, method=%s",
                     (unsigned)frf_bm_double_active,
                     (unsigned)frf_bm_flip_success,
                     (unsigned)frf_bm_flip_failure,frf_flip_mode_name());
            frf_perf_snapshot prof={};
            frf_perf_get_snapshot(&prof);
            if(prof.valid)
                snprintf(gLastCpuProfile,sizeof(gLastCpuProfile),
                    "MAME(avg %uf): logic=%luus update=%luus sound=%luus "
                    "tiles=%luus OSD=%luus palette=%luus",
                    prof.frames,prof.core_us,prof.update_us,
                    prof.sound_us,prof.video_us,prof.osd_us,prof.palette_us);
            else
                snprintf(gLastCpuProfile,sizeof(gLastCpuProfile),
                         "MAME subsystem timing not yet available");
            {
                unsigned total=frf_skip_drawn()+frf_skip_dropped();
                char skipStats[118];
                snprintf(skipStats,sizeof(skipStats),
                    "Skip mode=%s level=%u rendered=%u skipped=%u emufps=%u.%03u",
                    frf_skip_name(gGameSkip[id]),frf_skip_level(),
                    frf_skip_drawn(),frf_skip_dropped(),
                    frf_skip_measured_hz()/1000u,frf_skip_measured_hz()%1000u);
                (void)total;
                /* Save in the existing video summary for display and export. */
                snprintf(gPerfExportStatus,sizeof(gPerfExportStatus),"%.94s",skipStats);
            }
            /* Once per EXIT, never during gameplay. Enables offline
               Final Fight profiling even when the user forgets 'P'. */
            frf_export_last_perf();
            log_enableStdOut(1);
            printf("run_game(%s) returned %d\n",drivers[id]->name,rc);
            bm_log_end(rc);
            frf_record_result((unsigned)id,rc);
            /* No SD scan after a game. Manual R refreshes ROM list. */
            if(selected>=frf_game_count) selected=frf_game_count?frf_game_count-1:0;
            gMenuHeld=0;
            gMenuActive=true;
            lastSelected=-1;
        }
        mTimer.MsDelay(20);
    }
    return ShutdownHalt;
}

void CKernel::KeyStatusHandlerRaw(unsigned char modifiers, const unsigned char rawKeys[6])
{
    ++gUsbKeyboardReports;
    bm_input_set_keyboard(modifiers,rawKeys);
    /* USB HID: L/R Ctrl=0x01/0x10, L/R Shift=0x02/0x20,
       F11 usage=0x44. Edge triggered; no reboot in IRQ callback. */
    unsigned chord= ((modifiers & 0x11u)!=0) &&
                    ((modifiers & 0x22u)!=0) &&
                    frf_menu_has_key(rawKeys,0x44u);
    if(chord && !gRebootComboHeld)
        __atomic_store_n(&gRebootPending,1,__ATOMIC_RELEASE);
    gRebootComboHeld=chord;
    if(gMenuActive) {
        unsigned key=0;
        if(frf_menu_has_key(rawKeys,0x52)) key=1;       /* Up */
        else if(frf_menu_has_key(rawKeys,0x51)) key=2;  /* Down */
        else if(frf_menu_has_key(rawKeys,0x4f)) key=14; /* Right/frameskip */
        else if(frf_menu_has_key(rawKeys,0x50)) key=15; /* Left/frameskip */
        else if(frf_menu_has_key(rawKeys,0x4b)) key=5;  /* Page Up */
        else if(frf_menu_has_key(rawKeys,0x4e)) key=6;  /* Page Down */
        else if(frf_menu_has_key(rawKeys,0x2b)) key=16; /* TAB filter */
        else if(frf_menu_has_key(rawKeys,0x28)) key=3;  /* Enter */
        else if(frf_menu_has_key(rawKeys,0x15)) key=4; /* R */
        else if(frf_menu_has_key(rawKeys,0x0e)) key=14; /* K frameskip */
        else if(frf_menu_has_key(rawKeys,0x19)) key=8; /* V */
        else if(frf_menu_has_key(rawKeys,0x05)) key=10; /* B: flip A/B test */
        else if(frf_menu_has_key(rawKeys,0x17)) key=11; /* T: tear test */
        else if(frf_menu_has_key(rawKeys,0x13)) key=13; /* P: export test */
        else if(frf_menu_has_key(rawKeys,0x08)) key=12; /* E: export modes */
        else if(frf_menu_has_key(rawKeys,0x29)) key=9; /* ESC */
#ifdef FRF_MAME_AUDIO_FAST
        else if(frf_menu_has_key(rawKeys,0x09)) key=7; /* F: toggle audio engine */
#endif
        /* Any ordinary letter jumps alphabetically. Shift+letter
           overrides special launcher hotkeys, e.g. Shift+T for titles. */
        for(unsigned i=0;i<6u;i++){
            unsigned hid=rawKeys[i];
            if(hid<0x04u || hid>0x1du)continue;
            const unsigned ch=hid-0x04u;
            if(!key || (modifiers&0x22u))key=100u+ch;
            break;
        }
        if(key && key!=gMenuHeld)
            __atomic_store_n(&gMenuEvent,key,__ATOMIC_RELEASE);
        gMenuHeld=key;
    }
}

void CKernel::KeyboardRemovedHandler(CDevice *, void *)
{
    if(sThis) {
        unsigned char none[6]={0,0,0,0,0,0};
        bm_input_set_keyboard(0,none);
        sThis->mKeyboard=0;
        gMenuHeld=0;
        gRebootComboHeld=0;
        ++gUsbKbdRemove;
    }
}

void CKernel::GamePadStatusHandler(unsigned, const TGamePadState *state)
{
    if(!state) return;
    ++gUsbGamepadReports;
    uint32_t m=0;
    if(state->buttons & GamePadButtonLeft) m|=BM_PAD_LEFT;
    if(state->buttons & GamePadButtonRight) m|=BM_PAD_RIGHT;
    if(state->buttons & GamePadButtonUp) m|=BM_PAD_UP;
    if(state->buttons & GamePadButtonDown) m|=BM_PAD_DOWN;
    /* Known pads expose Circle's normalized button-bit table; generic
       HID pads may report raw button bits and need a separate mapping. */
    bool known=sThis && sThis->mGamePad &&
               ((sThis->mGamePad->GetProperties() & GamePadPropertyIsKnown)!=0);
    if(known) {
        if(state->buttons & GamePadButtonA) m|=BM_PAD_B1;
        if(state->buttons & GamePadButtonB) m|=BM_PAD_B2;
        if(state->buttons & GamePadButtonX) m|=BM_PAD_B3;
        if(state->buttons & GamePadButtonY) m|=BM_PAD_B4;
        if(state->buttons & GamePadButtonLB) m|=BM_PAD_B5;
        if(state->buttons & GamePadButtonRB) m|=BM_PAD_B6;
        if(state->buttons & GamePadButtonLT) m|=BM_PAD_B7;
        if(state->buttons & GamePadButtonRT) m|=BM_PAD_B8;
        if(state->buttons & GamePadButtonL3) m|=BM_PAD_B9;
        if(state->buttons & GamePadButtonR3) m|=BM_PAD_B10;
        if(state->buttons & (GamePadButtonStart|GamePadButtonPlus)) m|=BM_PAD_START;
        if(state->buttons & (GamePadButtonSelect|GamePadButtonMinus)) m|=BM_PAD_SELECT;
    } else {
        const unsigned b=state->buttons;
        if(b & (1u<<0)) m|=BM_PAD_B1;
        if(b & (1u<<1)) m|=BM_PAD_B2;
        if(b & (1u<<2)) m|=BM_PAD_B3;
        if(b & (1u<<3)) m|=BM_PAD_B4;
        if(b & (1u<<4)) m|=BM_PAD_B5;
        if(b & (1u<<5)) m|=BM_PAD_B6;
        if(b & (1u<<6)) m|=BM_PAD_B7;
        if(b & (1u<<7)) m|=BM_PAD_B8;
        if(b & (1u<<8)) m|=BM_PAD_B9;
        if(b & (1u<<9)) m|=BM_PAD_B10;
        if(b & (1u<<9)) m|=BM_PAD_START;
        if(b & (1u<<8)) m|=BM_PAD_SELECT;
    }
    /* Left stick digital dead zone (25%) covers modern pads that have no
       separate D-pad button report. */
    if(state->naxes>GamePadAxisLeftY) {
        for(unsigned a=0;a<2;a++) {
            const int min=state->axes[a].minimum;
            const int max=state->axes[a].maximum;
            if(max<=min)continue;
            const int mid=min+(max-min)/2;
            const int dz=(max-min)/4;
            const int val=state->axes[a].value;
            if(a==0) {
                if(val<mid-dz) m|=BM_PAD_LEFT;
                if(val>mid+dz) m|=BM_PAD_RIGHT;
            } else {
                if(val<mid-dz) m|=BM_PAD_UP;
                if(val>mid+dz) m|=BM_PAD_DOWN;
            }
        }
    }
    /* Standard HID hat 0..7, 8 means neutral. */
    if(state->nhats>0) {
        int hat=state->hats[0];
        if(hat==0 || hat==1 || hat==7) m|=BM_PAD_UP;
        if(hat==1 || hat==2 || hat==3) m|=BM_PAD_RIGHT;
        if(hat==3 || hat==4 || hat==5) m|=BM_PAD_DOWN;
        if(hat==5 || hat==6 || hat==7) m|=BM_PAD_LEFT;
    }
    bm_input_set_gamepad(m);
    if(gMenuActive) {
        static unsigned lastPadKey=0;
        unsigned key=0;
        if(m & BM_PAD_UP) key=1;
        else if(m & BM_PAD_DOWN) key=2;
        else if(m & BM_PAD_B5) key=5; /* LB pages up */
        else if(m & BM_PAD_B6) key=6; /* RB pages down */
        else if(m & BM_PAD_B4) key=16; /* B4 system filter */
        else if(m & BM_PAD_RIGHT) key=14;
        else if(m & BM_PAD_LEFT) key=15;
        else if(m & (BM_PAD_START|BM_PAD_B1)) key=3;
        else if(m & BM_PAD_SELECT) key=4;
        if(key && key!=lastPadKey)
            __atomic_store_n(&gMenuEvent,key,__ATOMIC_RELEASE);
        lastPadKey=key;
    }
}

void CKernel::GamePadRemovedHandler(CDevice *, void *)
{
    bm_input_set_gamepad(0);
    gMenuHeld=0;
    ++gUsbPadRemove;
    if(sThis) sThis->mGamePad=0;
}
