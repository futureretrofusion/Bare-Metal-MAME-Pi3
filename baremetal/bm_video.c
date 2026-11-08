#include "bm_platform.h"
#include <stddef.h>
#include <string.h>

extern volatile unsigned frf_bm_z80_cycles;
extern volatile unsigned frf_bm_vram_writes;
volatile unsigned frf_bm_frames=0;
volatile unsigned frf_bm_nonzero_sample=0;
volatile unsigned frf_bm_palette_sample=0;
static uint16_t *g_fb;
static uint32_t g_w, g_h, g_pitch;
/* Geometry diagnostics: zero-cost per frame unless a source cannot fit
 * the fixed HDMI framebuffer. Exposed to end-of-game CSV report. */
volatile uint32_t frf_bm_source_w=0,frf_bm_source_h=0,frf_bm_source_depth=0;
volatile uint32_t frf_bm_fb_width=0,frf_bm_fb_height=0;
volatile uint32_t frf_bm_oversize_frames=0;
/* A true VideoCore double-height framebuffer is allocated by Circle.
   g_page is always the hidden page; a successful flip makes it visible. */
static unsigned g_has_double=0, g_page=1, g_in_game=0, g_flips_ok=0;
static int (*g_flip)(unsigned page)=0;
volatile uint32_t frf_bm_render_us=0;
volatile uint32_t frf_bm_scanout_us=0;
volatile uint32_t frf_bm_vsync_us=0;
volatile uint32_t frf_bm_flip_success=0;
volatile uint32_t frf_bm_flip_failure=0;
volatile uint32_t frf_bm_double_active=0;
volatile uint32_t frf_bm_present_page=0;
volatile uint64_t frf_bm_first_present_us=0;
volatile uint64_t frf_bm_last_present_us=0;
volatile uint32_t frf_bm_max_present_gap_us=0;
extern uint64_t bm_cycles(void);
#ifdef FRF_BM_ASYNC_RENDER
/* Snapshot MAME's bitmap/palette on CPU0, then let CPU2 scale + VBlank.
   The live MAME bitmap is NOT safe to share while emulation advances. */
#define FRF_SNAPSHOT_MAX (4u*1024u*1024u)
static uint8_t g_fr_snapshot[FRF_SNAPSHOT_MAX] __attribute__((aligned(64)));
static uint32_t g_fr_pal_snapshot[65536] __attribute__((aligned(64)));
static volatile uint32_t g_fr_request=0,g_fr_finished=0;
static unsigned g_fr_pending=0;
volatile uint32_t frf_bm_snapshot_us=0;
static void bm_finish_async_render(void)
{
    if(!g_fr_pending)return;
    uint32_t want=__atomic_load_n(&g_fr_request,__ATOMIC_RELAXED);
    while(__atomic_load_n(&g_fr_finished,__ATOMIC_ACQUIRE)!=want)
        __asm__ volatile("wfe");
    g_fr_pending=0;
}
#endif

static uint8_t g_page_cleared[2]={0,0};
static unsigned g_scan = 96;
static unsigned g_orientation = 5; /* ROT90: proven Donkey Kong baseline */
static uint16_t g_shade_lut[65536] __attribute__((aligned(64)));
static uint16_t g_palette565[65536] __attribute__((aligned(64)));
static uint16_t g_palette565_dark[65536] __attribute__((aligned(64)));
static const uint32_t *g_palette_source=0;
static unsigned g_palette_count=0, g_palette_ready=0;
static bm_frame_job g_job __attribute__((aligned(64)));

static inline uint16_t rgb565(uint32_t c)
{
    return (uint16_t)((((c>>19)&31u)<<11)|(((c>>10)&63u)<<5)|((c>>3)&31u));
}
static inline uint16_t darken565(uint16_t p, unsigned s)
{
    unsigned r=(p>>11)&31u, g=(p>>5)&63u, b=p&31u;
    r=(r*(255u-s))>>8; g=(g*(255u-s))>>8; b=(b*(255u-s))>>8;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
#ifdef FRF_BM_ASYNC_VSYNC
/* Experimental CPU2 presenter. CPU0 owns all framebuffer writes; CPU2
   only waits for VBlank and asks firmware to switch virtual pages. */
static volatile uint32_t g_flip_req=0,g_flip_done=0,g_flip_result=0,g_flip_duration=0;
static volatile uint32_t g_flip_target=0;
static volatile uint64_t g_flip_stamp=0;
static unsigned g_flip_pending=0;
void bm_flip_worker_loop(void)
{
    uint32_t seen=0;
    for(;;){
        uint32_t req=__atomic_load_n(&g_flip_req,__ATOMIC_ACQUIRE);
        if(req==seen){__asm__ volatile("wfe");continue;}
        unsigned page=__atomic_load_n(&g_flip_target,__ATOMIC_RELAXED);
        uint64_t begin=bm_cycles();
        int ok=g_flip ? g_flip(page) : 0;
        uint64_t finish=bm_cycles();
        g_flip_result=ok?1u:0u;
        g_flip_duration=(uint32_t)(finish-begin);
        g_flip_stamp=finish;
        __atomic_store_n(&g_flip_done,req,__ATOMIC_RELEASE);
        seen=req;
        __asm__ volatile("sev");
    }
}
static void bm_finish_async_flip(void)
{
    if(!g_flip_pending)return;
    unsigned req=__atomic_load_n(&g_flip_req,__ATOMIC_RELAXED);
    while(__atomic_load_n(&g_flip_done,__ATOMIC_ACQUIRE)!=req)
        __asm__ volatile("wfe");
    frf_bm_vsync_us+=g_flip_duration;
    if(g_flip_result) {
        ++g_flips_ok;
        ++frf_bm_flip_success;
        frf_bm_present_page=g_page;
        g_page^=1u;
        uint64_t t=g_flip_stamp;
        if(!frf_bm_first_present_us) frf_bm_first_present_us=t;
        if(frf_bm_last_present_us) {
            uint64_t dt=t-frf_bm_last_present_us;
            if(dt>frf_bm_max_present_gap_us)
                frf_bm_max_present_gap_us=(uint32_t)(dt>0xffffffffull?0xffffffffull:dt);
        }
        frf_bm_last_present_us=t;
    } else {
        ++frf_bm_flip_failure;
        if(!g_flips_ok){
            memcpy((uint8_t*)g_fb,(uint8_t*)g_fb+(size_t)g_pitch*g_h,
                   (size_t)g_pitch*g_h);
            g_has_double=0;
            frf_bm_double_active=0;
            g_page=0;
        }
    }
    g_flip_pending=0;
}
#endif
void bm_video_attach(void *fb, uint32_t width, uint32_t height, uint32_t pitch)
{
    g_fb=(uint16_t*)fb;g_w=width;g_h=height;g_pitch=pitch;
    frf_bm_fb_width=width;frf_bm_fb_height=height;
}
void bm_video_configure_double(uint32_t virt_h, uint32_t allocated_size,
                               int (*flip)(unsigned page))
{
    g_has_double= (g_fb && g_w && g_h && g_pitch>=g_w*2u &&
                   virt_h>=g_h*2u &&
                   (uint64_t)allocated_size >= (uint64_t)g_pitch*g_h*2u &&
                   flip!=NULL);
    g_flip=flip;
    g_page=1;
    g_flips_ok=0;
    g_page_cleared[0]=g_page_cleared[1]=0;
    frf_bm_double_active=g_has_double;
    frf_bm_flip_success=frf_bm_flip_failure=0;
    frf_bm_present_page=0;
}
void bm_video_begin_game(void)
{
#ifdef FRF_BM_ASYNC_RENDER
    bm_finish_async_render();
#endif
    g_palette_ready=0;g_palette_source=0;g_palette_count=0;
    g_in_game=1;
    g_page=1; /* console/menu still displayed on page 0 */
    g_flips_ok=0;
    g_page_cleared[0]=g_page_cleared[1]=0;
    frf_bm_double_active=g_has_double;
    frf_bm_flip_success=frf_bm_flip_failure=0;
    frf_bm_present_page=0;
    frf_bm_frames=0;
    frf_bm_source_w=frf_bm_source_h=frf_bm_source_depth=0;
    frf_bm_oversize_frames=0;
    frf_bm_render_us=frf_bm_scanout_us=frf_bm_vsync_us=0;
#ifdef FRF_BM_ASYNC_RENDER
    frf_bm_snapshot_us=0;
#endif
    frf_bm_first_present_us=frf_bm_last_present_us=0;
    frf_bm_max_present_gap_us=0;
}
int bm_video_is_double(void) {return g_has_double?1:0;}
void bm_video_end_game(void)
{
#ifdef FRF_BM_ASYNC_RENDER
    bm_finish_async_render();
#endif
#ifdef FRF_BM_ASYNC_VSYNC
    bm_finish_async_flip();
#endif
    g_in_game=0;
    if(g_has_double && g_flip) g_flip(0); /* restore console page */
    g_page=1;
    frf_bm_present_page=0;
}
void bm_video_set_scanlines(unsigned strength)
{
    g_scan=strength>224?224:strength;
    /* The old scanout multiplied RGB components per pixel, per frame.
       Precalculate exact 565 attenuation (128 KiB) once on boot. */
    if(g_scan)
        for(unsigned c=0;c<65536u;c++)
            g_shade_lut[c]=darken565((uint16_t)c,g_scan);
    /* Invalidate derived indexed shade palette on scanline setting change. */
    g_palette_ready=0;
}
void bm_video_set_rotation(unsigned clockwise) { g_orientation=clockwise?5:0; }
void bm_video_set_orientation(unsigned flags) { g_orientation=flags&7u; }
void bm_debug_fill(uint16_t color)
{
    if(!g_fb) return;
    for(unsigned y=0;y<g_h;y++) {
        uint16_t *d=(uint16_t*)((uint8_t*)g_fb+y*g_pitch);
        for(unsigned x=0;x<g_w;x++) d[x]=color;
    }
}


/* Direct framebuffer progress marker: independent of MAME screen callbacks.
   12 pixel high top strip: stage colour and a moving white square. */
void bm_debug_progress(unsigned stage, unsigned heartbeat)
{
    static const uint16_t colors[] = {
        0xf800u, 0xffe0u, 0x07e0u, 0x07ffu, 0x001fu, 0xf81fu, 0xffffu
    };
    /* v0.32: the old CPU/VRAM debugging bars are intentionally disabled.
       SD MAMELOG.TXT remains enabled for diagnostics. */
    (void)stage; (void)heartbeat;
    return;
    unsigned h=g_h<12u?g_h:12u;
    uint16_t c=colors[stage % (sizeof colors/sizeof colors[0])];
    unsigned block=heartbeat % ((g_w+15u)/16u);
    for(unsigned y=0;y<h;y++) {
        uint16_t *d=(uint16_t*)((uint8_t*)g_fb+y*g_pitch);
        uint16_t line_color = y<4u ? (frf_bm_z80_cycles ? 0x07e0u : 0xf800u)
                              : y<8u ? (frf_bm_vram_writes ? 0x001fu : 0xf800u)
                              : (frf_bm_frames>1u ? 0xffe0u : 0xf800u);
        for(unsigned x=0;x<g_w;x++)
            d[x]=((x/16u)==block)?0xffffu:line_color;
    }
}


/* v0.61: convert indexed palette only when it changes.
   Avoid RGB888->RGB565 math for every source pixel on every frame. */
void bm_video_update_palette(const uint32_t *palette,uint32_t entries,unsigned dirty)
{
#ifdef FRF_BM_ASYNC_RENDER
    /* CPU2 reads the derived LUT until completion. No concurrent mutation. */
    bm_finish_async_render();
#endif
    if(!palette || !entries) {
        g_palette_ready=0; g_palette_source=0; g_palette_count=0;
        return;
    }
    unsigned n=entries>65536u?65536u:entries;
    if(!dirty && g_palette_ready && palette==g_palette_source && n==g_palette_count)return;
    for(unsigned i=0;i<n;i++){
        uint16_t c=rgb565(palette[i]);
        g_palette565[i]=c;
        g_palette565_dark[i]=g_scan?g_shade_lut[c]:c;
    }
    g_palette_source=palette;
    g_palette_count=n;
    g_palette_ready=1;
}
static inline uint16_t fetch565(const bm_frame_job *j, unsigned x, unsigned y)
{
    const uint8_t *row=(const uint8_t*)j->src+y*j->src_pitch;
    if(j->src_depth==16) {
        uint16_t v=((const uint16_t*)row)[x];
        if(g_palette_ready && v<g_palette_count) return g_palette565[v];
        if(j->palette && v<j->palette_entries) return rgb565(j->palette[v]);
        return v;
    }
    if(j->src_depth==15) {
        uint16_t v=((const uint16_t*)row)[x];
        return (uint16_t)(((v&0x7c00u)<<1)|((v&0x03e0u)<<1)|(v&0x001fu));
    }
    if(j->src_depth==32) return rgb565(((const uint32_t*)row)[x]);
    return 0;
}
/* v0.52: draw only the integer-scaled game rectangle directly into
   VideoCore's hidden page. Previously 1920x1080 staging was cleared,
   then EVERY HDMI pixel was copied to the framebuffer each frame.
   That dominates CPU bandwidth on Pi 3 and deprives arcade CPUs of cycles.
   Each page is cleared ONCE for black borders, not every frame. */

static void clear_hidden_once(unsigned page)
{
    if(!g_fb || page>1 || g_page_cleared[page])return;
    uint8_t *fb=(uint8_t*)g_fb+(size_t)g_pitch*g_h*page;
    memset(fb,0,(size_t)g_pitch*g_h);
    g_page_cleared[page]=1;
}
static void direct_integer_scanout(const bm_frame_job *j)
{
    if(!g_fb || !j->src || !j->src_w || !j->src_h)return;
    const unsigned s=j->integer_scale?j->integer_scale:1;
    const unsigned swapped=(g_orientation & 4u)!=0;
    const unsigned ow=(swapped?j->src_h:j->src_w)*s;
    const unsigned oh=(swapped?j->src_w:j->src_h)*s;
    if(ow>g_w || oh>g_h)return;
    const unsigned ox=(g_w-ow)/2u, oy=(g_h-oh)/2u;
    const unsigned pg=(g_has_double && g_in_game)?g_page:0;
    clear_hidden_once(pg);
    uint8_t *fb=(uint8_t*)g_fb+(size_t)g_pitch*g_h*pg;
    /* Common arcade 2x path: ONE palette fetch/rotation per native pixel,
       then write two physical rows (second 25% darker). No divisions
       inside the inner pixel loop, no full-HD staging, no screen-size copy. */
    if(s==2u) {
        const unsigned native_w=ow/2u, native_h=oh/2u;
        /* v0.96: common CPS1/16-bit indexed unrotated fast path.
           Per pixel: single index read, two palette reads, four stores.
           Eliminates orientation checks and RGB565 shade LUT lookup. */
        if(g_orientation==0u && j->src_depth==16u && g_palette_ready &&
           (j->palette==g_palette_source
#ifdef FRF_BM_ASYNC_RENDER
            || j->palette==g_fr_pal_snapshot
#endif
            ) && j->palette_entries<=g_palette_count) {
            for(unsigned y=0;y<native_h;y++){
                const uint16_t *src=(const uint16_t*)((const uint8_t*)j->src+
                                                     (size_t)y*j->src_pitch);
                uint16_t *top=(uint16_t*)(fb+(size_t)(oy+y*2u)*g_pitch)+ox;
                uint16_t *bottom=(uint16_t*)(fb+(size_t)(oy+y*2u+1u)*g_pitch)+ox;
                for(unsigned x=0;x<native_w;x++){
                    unsigned idx=src[x];
                    if(idx>=g_palette_count){
                        uint16_t v=fetch565(j,x,y),dark=g_scan?g_shade_lut[v]:v;
                        top[x*2u]=top[x*2u+1u]=v;
                        bottom[x*2u]=bottom[x*2u+1u]=dark;
                    } else {
                        uint16_t v=g_palette565[idx],dark=g_palette565_dark[idx];
                        top[x*2u]=top[x*2u+1u]=v;
                        bottom[x*2u]=bottom[x*2u+1u]=dark;
                    }
                }
            }
            return;
        }
        for(unsigned y=0;y<native_h;y++) {
            uint16_t *top=(uint16_t*)(fb+(size_t)(oy+y*2u)*g_pitch)+ox;
            uint16_t *bottom=(uint16_t*)(fb+(size_t)(oy+y*2u+1u)*g_pitch)+ox;
            for(unsigned x=0;x<native_w;x++) {
                unsigned srcx=swapped?y:x;
                unsigned srcy=swapped?x:y;
                if(g_orientation&1u) {
                    if(swapped)srcy=j->src_h-1u-srcy;
                    else srcx=j->src_w-1u-srcx;
                }
                if(g_orientation&2u) {
                    if(swapped)srcx=j->src_w-1u-srcx;
                    else srcy=j->src_h-1u-srcy;
                }
                uint16_t v=fetch565(j,srcx,srcy);
                uint16_t dark=g_scan?g_shade_lut[v]:v;
                top[x*2u]=top[x*2u+1u]=v;
                bottom[x*2u]=bottom[x*2u+1u]=dark;
            }
        }
        return;
    }
    /* 1x fallback, for resolutions that cannot fit at 2x. */
    for(unsigned y=0;y<oh;y++) {
        uint16_t *dst=(uint16_t*)(fb+(size_t)(oy+y)*g_pitch)+ox;
        for(unsigned x=0;x<ow;x++) {
            unsigned srcx=swapped?y:x;
            unsigned srcy=swapped?x:y;
            if(g_orientation&1u) {
                if(swapped)srcy=j->src_h-1u-srcy;
                else srcx=j->src_w-1u-srcx;
            }
            if(g_orientation&2u) {
                if(swapped)srcx=j->src_w-1u-srcx;
                else srcy=j->src_h-1u-srcy;
            }
            uint16_t v=fetch565(j,srcx,srcy);
            dst[x]=(g_scan&&(y&1u))?g_shade_lut[v]:v;
        }
    }
}
static void scale_frame(const bm_frame_job *j)
{
    direct_integer_scanout(j);
}
static void scanout(void)
{
    /* Scanout is fused with integer scaling: no full-screen copy. */
}
#ifdef FRF_BM_ASYNC_RENDER
/* CPU2 owns render -> mailbox flip -> VBlank fence, serialised by seq/ack.
   CPU0 emulates the following frame while this worker presents the last. */
void bm_async_render_worker_loop(void)
{
    uint32_t seen=0;
    for(;;){
        uint32_t seq=__atomic_load_n(&g_fr_request,__ATOMIC_ACQUIRE);
        if(seq==seen){__asm__ volatile("wfe");continue;}
        uint64_t t0=bm_cycles();
        scale_frame(&g_job);
        uint64_t t1=bm_cycles();
        int presented=0;
        if(g_has_double && g_in_game && g_flip){
            __sync_synchronize();
            if(g_flip(g_page)){
                ++g_flips_ok;++frf_bm_flip_success;presented=1;
                frf_bm_present_page=g_page;g_page^=1u;
            } else {
                ++frf_bm_flip_failure;
                if(!g_flips_ok){
                    memcpy((uint8_t*)g_fb,
                           (uint8_t*)g_fb+(size_t)g_pitch*g_h,
                           (size_t)g_pitch*g_h);
                    g_has_double=0;frf_bm_double_active=0;g_page=0;
                }
            }
        }
        uint64_t t2=bm_cycles();
        if(presented){
            if(!frf_bm_first_present_us)frf_bm_first_present_us=t2;
            if(frf_bm_last_present_us){
                uint64_t dt=t2-frf_bm_last_present_us;
                if(dt>frf_bm_max_present_gap_us)
                    frf_bm_max_present_gap_us=(uint32_t)
                       (dt>0xffffffffull?0xffffffffull:dt);
            }
            frf_bm_last_present_us=t2;
        }
        frf_bm_render_us+=(uint32_t)(t1-t0);
        frf_bm_vsync_us+=(uint32_t)(t2-t1);
        __atomic_store_n(&g_fr_finished,seq,__ATOMIC_RELEASE);
        seen=seq;
        __asm__ volatile("sev");
    }
}
#endif
void bm_submit_frame(const void *src, uint32_t w, uint32_t h, uint32_t pitch, uint32_t depth,
                     const uint32_t *palette, uint32_t palette_entries)
{
#ifdef FRF_BM_ASYNC_RENDER
    /* Also handles tear-test frames where no new palette callback occurs. */
    bm_finish_async_render();
#endif
    unsigned swapped=(g_orientation & 4u)!=0;
    unsigned sw=swapped?h:w, sh=swapped?w:h;
    frf_bm_source_w=w;frf_bm_source_h=h;frf_bm_source_depth=depth;
    if(sw>g_w || sh>g_h || !g_fb || !g_w || !g_h){
        ++frf_bm_oversize_frames;
        return; /* exact cause recorded instead of a silent black frame */
    }
    unsigned sx=sw?g_w/sw:1, sy=sh?g_h/sh:1, is=sx<sy?sx:sy; if(is<1)is=1; if(is>2u)is=2u; /* FRF v0.51: small integer presentation; never zoom beyond 2x */
    g_job.src=src; g_job.palette=palette; g_job.palette_entries=palette_entries;
    g_job.src_w=w; g_job.src_h=h; g_job.src_pitch=pitch; g_job.src_depth=depth;
    g_job.dst=g_fb; g_job.dst_w=g_w; g_job.dst_h=g_h; g_job.dst_pitch=g_pitch;
    g_job.integer_scale=is; g_job.scanline_strength=g_scan;
    frf_bm_frames++;
#ifndef FRF_MAME_FAST
    if(src && depth==16 && w && h) {
        unsigned seen=0;
        for(unsigned y=0;y<h;y+=8) {
            const uint16_t *line=(const uint16_t *)((const uint8_t *)src+y*pitch);
            for(unsigned x=0;x<w;x+=8) if(line[x]) seen++;
        }
        frf_bm_nonzero_sample=seen;
    }
    frf_bm_palette_sample = (palette && palette_entries) ? palette[palette_entries>1 ? 1 : 0] : 0;
#endif
#ifdef FRF_BM_ASYNC_RENDER
    /* Copy MAME's mutable bitmap before the next emulated frame can reuse it.
       Snapshot palette for fallback lookup; derived LUT is fenced above. */
    uint64_t frame_bytes=(uint64_t)pitch*h;
    if(g_has_double && g_in_game && src && pitch && h &&
       frame_bytes<=FRF_SNAPSHOT_MAX &&
       palette_entries<=65536u){
        uint64_t copy_start=bm_cycles();
        memcpy(g_fr_snapshot,src,(size_t)frame_bytes);
        if(palette && palette_entries)
            memcpy(g_fr_pal_snapshot,palette,(size_t)palette_entries*4u);
        g_job.src=g_fr_snapshot;
        g_job.palette=palette?g_fr_pal_snapshot:0;
        frf_bm_snapshot_us+=(uint32_t)(bm_cycles()-copy_start);
        __sync_synchronize();
        uint32_t seq=__atomic_load_n(&g_fr_request,__ATOMIC_RELAXED)+1u;
        g_fr_pending=1;
        __atomic_store_n(&g_fr_request,seq,__ATOMIC_RELEASE);
        __asm__ volatile("sev");
        return;
    }
    /* Unusually large frame: synchronously process it on CPU0 instead. */
#endif
    /* FRF v0.46: ONLY CPU0 owns g_stage and video scanout.
       No worker producer/consumer races, no duplicate frame copies. */
#ifdef FRF_BM_ASYNC_VSYNC
    bm_finish_async_flip(); /* previous VBlank completed before reusing that page */
#endif
    uint64_t t0=bm_cycles();
    scale_frame(&g_job);
    uint64_t t1=bm_cycles();
    scanout();
    uint64_t t2=bm_cycles();
    int presented=0;
#ifdef FRF_BM_ASYNC_VSYNC
    if(g_has_double && g_in_game && g_flip){
        __sync_synchronize();
        __atomic_store_n(&g_flip_target,g_page,__ATOMIC_RELAXED);
        g_flip_pending=1;
        uint32_t seq=__atomic_load_n(&g_flip_req,__ATOMIC_RELAXED)+1u;
        __atomic_store_n(&g_flip_req,seq,__ATOMIC_RELEASE);
        __asm__ volatile("sev");
    }
#else
    if(g_has_double && g_in_game && g_flip) {
        /* Complete CPU frame writes before the VideoCore sees the new page. */
        __sync_synchronize();
        if(g_flip(g_page)) {
            g_flips_ok++;
            frf_bm_flip_success++;
            presented=1;
            frf_bm_present_page=g_page;
            g_page^=1u;
        } else {
            frf_bm_flip_failure++;
            if(!g_flips_ok) {
            /* Some firmware versions reject virtual-offset flips.
               On first failure, copy the completed hidden page onto
               visible page zero and fall back to single buffering. */
            memcpy((uint8_t *)g_fb,
                   (uint8_t *)g_fb+(size_t)g_pitch*g_h,
                   (size_t)g_pitch*g_h);
            g_has_double=0;
            frf_bm_double_active=0;
            g_page=0;
            }
        }
        /* A transient failure after an earlier flip leaves the back page
           intact; retry next frame rather than corrupt the visible page. */
    }
#endif
    uint64_t t3=bm_cycles();
    if(presented) {
        if(!frf_bm_first_present_us) frf_bm_first_present_us=t3;
        if(frf_bm_last_present_us) {
            uint64_t dt=t3-frf_bm_last_present_us;
            if(dt>frf_bm_max_present_gap_us) frf_bm_max_present_gap_us=(uint32_t)(dt>0xffffffffull?0xffffffffull:dt);
        }
        frf_bm_last_present_us=t3;
    }
    frf_bm_render_us+=(uint32_t)(t1-t0);
    frf_bm_scanout_us+=(uint32_t)(t2-t1);
    frf_bm_vsync_us+=(uint32_t)(t3-t2);
}
void bm_workers_init(void) { g_job.seq=g_job.scaled_seq=g_job.done=0; }
void bm_worker_loop(unsigned cpu_id)
{
    uint32_t seen=0;
    for(;;) {
        uint32_t seq=__atomic_load_n(&g_job.seq,__ATOMIC_ACQUIRE);
        if(seq==seen) { __asm__ volatile("yield"); continue; }
        if(cpu_id==BM_CORE_SCALE) {
            scale_frame(&g_job);
            __atomic_store_n(&g_job.scaled_seq,seq,__ATOMIC_RELEASE);
        } else if(cpu_id==BM_CORE_SCANOUT) {
            while(__atomic_load_n(&g_job.scaled_seq,__ATOMIC_ACQUIRE)<seq) __asm__ volatile("yield");
            scanout();
            __atomic_store_n(&g_job.done,seq,__ATOMIC_RELEASE);
        }
        seen=seq;
    }
}
