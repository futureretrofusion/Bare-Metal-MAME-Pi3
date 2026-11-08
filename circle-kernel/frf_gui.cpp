#include "frf_gui.h"
#include "frf_gui_font.h"
#include "frf_gui_banner.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* FRF Pi3 bare-metal framebuffer GUI, RGB565, CPU0 only.
 * No GPU driver, graphics context, SD assets, threads or allocation.
 * Model is independent from rendering: thumbnail and attract media
 * providers can be added without changing the game index. */
static uint16_t *g_fb_base=0;
static unsigned g_gui_front=0,g_gui_double=0,g_gui_failed=0;
static int (*g_gui_flip)(unsigned page)=0;
static uint16_t *g_pixels=0;
static unsigned g_w=0,g_h=0,g_stride=0,g_depth=0;
static uint16_t rgb(unsigned r,unsigned g,unsigned b){
    return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static uint16_t C_BG,C_TOP,C_PANEL,C_EDGE,C_TEXT,C_MUTED,C_ACCENT,C_SELECTED,C_GOOD,C_WARN,C_RED;
void frf_gui_attach(void *p,unsigned w,unsigned h,unsigned pitch,unsigned depth){
    g_fb_base=(uint16_t*)p;g_pixels=g_fb_base;
    g_w=w;g_h=h;g_stride=pitch/2u;g_depth=depth;
    g_gui_front=0;g_gui_double=0;g_gui_failed=0;g_gui_flip=0;
    C_BG=rgb(8,14,27);C_TOP=rgb(13,25,43);
    C_PANEL=rgb(17,30,51);C_EDGE=rgb(41,60,83);
    C_TEXT=rgb(236,244,250);C_MUTED=rgb(138,164,191);
    C_ACCENT=rgb(65,217,242);C_SELECTED=rgb(22,67,84);
    C_GOOD=rgb(93,226,175);C_WARN=rgb(249,192,80);
    C_RED=rgb(242,112,130);
}
int frf_gui_available(void){
    return g_pixels && g_w==1280u && g_h==720u &&
           g_depth==16u && g_stride>=1280u;
}
void frf_gui_configure_double(unsigned virt_height,unsigned fb_bytes,
                              int (*flip)(unsigned page)){
    /* Only use the second hardware scanout page when Circle actually
       allocated two complete frames. No extra RAM image allocation. */
    g_gui_double=(g_fb_base && g_w==1280u && g_h==720u &&
                  g_depth==16u && g_stride>=g_w &&
                  virt_height>=g_h*2u &&
                  (uint64_t)fb_bytes>=
                     (uint64_t)g_stride*g_h*2u &&
                  flip!=0)?1u:0u;
    g_gui_flip=flip;
    g_gui_front=0;
}
int frf_gui_is_double(void){return g_gui_double?1:0;}
unsigned frf_gui_failed_flips(void){return g_gui_failed;}
void frf_gui_before_game(void){
    /* bm_video_begin_game() assumes page 0 is visible, page 1 is hidden.
       If menu ends on page 1, restore page 0 *before* entering MAME.
       Copy the complete final menu while page 0 is hidden so no old
       console text or half-rendered menu becomes visible. */
    if(!g_gui_double || !g_gui_flip || !g_gui_front)return;
    size_t bytes=(size_t)g_stride*g_h*sizeof(uint16_t);
    memcpy(g_fb_base,g_fb_base+(size_t)g_gui_front*g_stride*g_h,bytes);
    __sync_synchronize();
    if(g_gui_flip(0u)){
        g_gui_front=0;
    }else{
        ++g_gui_failed;
        /* Safety fallback: request page 0 again; gameplay retains its
           original page-1 initialization and separate flip logic. */
        if(g_gui_flip(0u))g_gui_front=0;
    }
}
void frf_gui_after_game(void){
    /* bm_video_end_game() has restored the console scanout to page 0.
       The next menu redraw must target page 1, not the visible page. */
    g_gui_front=0;
    g_pixels=g_fb_base;
}
static void fill(int x,int y,int w,int h,uint16_t c){
    if(!g_pixels||w<=0||h<=0)return;
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x>=(int)g_w||y>=(int)g_h)return;
    if(x+w>(int)g_w)w=(int)g_w-x;
    if(y+h>(int)g_h)h=(int)g_h-y;
    if(w<=0||h<=0)return;
    for(int row=0;row<h;row++){
        uint16_t *dst=g_pixels+(size_t)(y+row)*g_stride+(unsigned)x;
        for(int col=0;col<w;col++)dst[col]=c;
    }
}
static void border(int x,int y,int w,int h,uint16_t c){
    fill(x,y,w,1,c);fill(x,y+h-1,w,1,c);
    fill(x,y,1,h,c);fill(x+w-1,y,1,h,c);
}
static void glyph(int x,int y,char c,uint16_t fg,unsigned scale){
    unsigned index=(unsigned char)c;
    if(index<32u||index>126u)index='?';
    const uint8_t *rows=frf_gui_font[index-32u];
    for(int a=0;a<13;a++){
        unsigned bits=rows[a];
        for(int b=0;b<8;b++){
            if(bits&(0x80u>>b))
                fill(x+b*(int)scale,y+a*(int)scale,
                     (int)scale,(int)scale,fg);
        }
    }
}
static void txt(int x,int y,const char*s,uint16_t c,unsigned scale,unsigned maxchars){
    if(!s||!scale)return;
    unsigned n=0;
    while(*s&&n<maxchars){
        glyph(x+(int)(n*9u*scale),y,*s++,c,scale);
        n++;
    }
}
static void pill(int x,int y,int w,const char *label,uint16_t c){
    fill(x,y,w,25,C_BG);border(x,y,w,25,c);
    txt(x+10,y+6,label,c,1,(unsigned)(w-16)/9u);
}
static void wrapped(int x,int y,const char *str,uint16_t color,unsigned maxcols,unsigned maxlines){
    if(!str)return;
    unsigned len=(unsigned)strlen(str);
    unsigned at=0;
    for(unsigned line=0;line<maxlines&&at<len;line++){
        char buf[72];unsigned k=0,lastspace=0;
        while(at+k<len&&k<maxcols&&k<sizeof(buf)-1u){
            char ch=str[at+k];
            buf[k]=ch;if(ch==' ')lastspace=k;k++;
        }
        if(at+k<len && lastspace>10u)k=lastspace;
        while(k&&buf[k-1]==' ')k--;
        buf[k]=0;
        txt(x,y+(int)line*23,buf,color,1,maxcols);
        at+=k;
        while(at<len&&str[at]==' ')at++;
    }
}
/* Direct zero-allocation logo blit; pixel data is embedded in ROM.
 * Original source aspect ratio 3:1 is preserved at 600x200 RGB565. */
static void frf_draw_banner(void){
    static const int left=29,top=7;
    for(int y=0;y<FRF_GUI_BANNER_H;y++){
        uint16_t *dst=g_pixels+(size_t)(top+y)*g_stride+(unsigned)left;
        const uint16_t *src=frf_gui_banner+(size_t)y*FRF_GUI_BANNER_W;
        memcpy(dst,src,FRF_GUI_BANNER_W*sizeof(uint16_t));
    }
}
static void bg(void){
    fill(0,0,(int)g_w,(int)g_h,C_BG);
    fill(0,0,1280,210,C_TOP);
    fill(0,207,1280,3,C_ACCENT);
    fill(0,650,1280,70,C_TOP);
    fill(0,650,1280,2,C_EDGE);
}
static const char *status_name(unsigned value){
    return value==1?"EXIT OK":value==2?"FAILED":
           value==3?"NO VIDEO":"UNTESTED";
}
static const char *rom_status_name(unsigned value){
    switch(value){
      case 1:return "ROM MATCH";
      case 2:return "RENAME NEEDED";
      case 3:return "INCOMPLETE";
      case 4:return "UNDUMPED";
      case 5:return "PROM OPTIONAL";
      default:return "NOT AUDITED";
    }
}
static uint16_t rom_status_color(unsigned value){
    return value==1?C_GOOD:value==3?C_RED:
           value==2||value==4||value==5?C_WARN:C_MUTED;
}

void frf_gui_render(const frf_gui_model *m){
    if(!frf_gui_available()||!m)return;
    const unsigned target=g_gui_double?(g_gui_front^1u):0u;
    g_pixels=g_fb_base+(size_t)target*g_stride*g_h;
    bg();
    frf_draw_banner();
    pill(1030,24,203,"BARE METAL PI3",C_ACCENT);
    txt(680,62,"MAME106RGB  /  HD ARCADE",C_TEXT,1,34);
    txt(680,89,"NEO GEO  *  CPS  *  CLASSICS",C_MUTED,1,39);
    txt(1092,87,"v1.15",C_MUTED,1,7);
    txt(680,140,"GAME LIBRARY",C_ACCENT,1,22);
    char sum[96];
    static const char *filter_name[]={"ALL ROMS","CRC VERIFIED","NEO GEO"};
    unsigned filter=m->filter_mode<3?m->filter_mode:0u;
    snprintf(sum,sizeof(sum),"%s: %u / %u     TAB = FILTER",
             filter_name[filter],m->count,m->total_count);
    txt(680,170,sum,C_MUTED,1,60);
    fill(43,211,789,435,C_PANEL);
    border(43,211,789,435,C_EDGE);
    for(unsigned k=0;k<FRF_GUI_ROWS;k++){
        if(k>=m->rows)break;
        const frf_gui_item *it=&m->items[k];
        if(!it->title)continue;
        int y=219+(int)k*30;
        int chosen=m->first+k==m->selected;
        if(chosen){
            fill(50,y,774,28,C_SELECTED);
            fill(50,y,5,28,C_ACCENT);
            border(50,y,774,28,C_ACCENT);
        }else if(k&1u){
            fill(50,y,774,28,rgb(20,35,57));
        }
        char rank[12];
        snprintf(rank,sizeof(rank),"%03u",m->first+k+1u);
        txt(66,y+8,rank,chosen?C_ACCENT:C_MUTED,1,4);
        txt(119,y+8,it->title,chosen?C_TEXT:C_TEXT,1,58);
        unsigned status=it->status;
        unsigned rs=it->rom_status;
        uint16_t sc=status==2?C_RED:status==3?C_WARN:
                    status==1?C_GOOD:rom_status_color(rs);
        const char *flag=status==2?"ERR":status==3?"NOF":
                          status==1?"RAN":
                          rs==1?"ROM":rs==2?"NAME":rs==3?"MISS":
                          rs==4?"MCU":rs==5?"OPT":"--";
        txt(747,y+8,flag,sc,1,4);
    }
    if(m->count>FRF_GUI_ROWS){
        int base=216,bar_h=420;
        fill(822,base,3,bar_h,C_EDGE);
        unsigned thumb=(unsigned)((uint64_t)bar_h*FRF_GUI_ROWS/m->count);
        if(thumb<18)thumb=18;
        unsigned thumb_y=(unsigned)((uint64_t)(bar_h-thumb)*m->first/
                       (m->count-FRF_GUI_ROWS));
        fill(821,base+(int)thumb_y,5,(int)thumb,C_ACCENT);
    }
    /* Right-hand details column */
    fill(852,211,386,435,C_PANEL);
    border(852,211,386,435,C_EDGE);
    txt(879,230,"GAME DETAILS",C_ACCENT,1,22);
    fill(879,253,330,1,C_EDGE);
    txt(879,264,m->selected_short?m->selected_short:"NO GAME",C_WARN,1,28);
    wrapped(879,285,m->selected_title?m->selected_title:"Insert ROM ZIPs to begin",
            C_TEXT,33,2);
    txt(879,328,"RELEASE",C_MUTED,1,12);
    txt(879,350,m->selected_year?m->selected_year:"--",C_TEXT,1,12);
    txt(1004,328,"PUBLISHER",C_MUTED,1,12);
    txt(1004,350,m->selected_maker?m->selected_maker:"--",C_TEXT,1,23);
    fill(879,374,330,1,C_EDGE);
    txt(879,385,"NATIVE VIDEO",C_MUTED,1,22);
    char spec[72];
    snprintf(spec,sizeof(spec),"%ux%u  /  %u.%03u Hz",
       m->native_width,m->native_height,
       m->native_millihz/1000u,m->native_millihz%1000u);
    txt(879,405,spec,C_TEXT,1,35);
    txt(879,433,"FRAMESKIP",C_MUTED,1,20);
    static const char *fs[]={"OFF","AUTO","SKIP 1","SKIP 2","SKIP 3","SKIP 4"};
    unsigned mode=m->frame_skip_mode<6u?m->frame_skip_mode:0u;
    pill(879,451,152,fs[mode],C_ACCENT);
    txt(879,485,"SESSION STATUS",C_MUTED,1,21);
    pill(879,504,145,status_name(m->selected_status),
         m->selected_status==1?C_GOOD:m->selected_status==2?C_RED:
         m->selected_status==3?C_WARN:C_MUTED);
    txt(879,538,"ROM ARCHIVE AUDIT",C_MUTED,1,23);
    pill(879,557,202,rom_status_name(m->selected_rom_status),
         rom_status_color(m->selected_rom_status));
    char padline[80];
    if(m->usb_pad_connected){
        snprintf(padline,sizeof(padline),"PAD %04X:%04X  HID:%s  RPTS:%u",
            m->usb_pad_vidpid>>16,m->usb_pad_vidpid&0xffffu,
            m->usb_pad_decoder==1?"MAP":
            m->usb_pad_decoder==3?"SWITCH":
            m->usb_pad_decoder==4?"SHANWAN":"RAW",m->usb_pad_reports);
        txt(879,607,padline,C_GOOD,1,38);
        snprintf(padline,sizeof(padline),"INPUT:%04X  REPORT ID:%u",
            m->usb_pad_mask&0xffffu,m->usb_pad_report_id);
        txt(879,626,padline,C_ACCENT,1,38);
    }else{
        snprintf(padline,sizeof(padline),"USB HOST:%s  MOUNT:%u  ERR:%u",
                 m->usb_host_ok?"ON":"OFF",
                 m->usb_device_count,m->usb_enum_errors);
        txt(879,607,padline,m->usb_host_ok?C_MUTED:C_RED,1,37);
        txt(879,626,"USB PAD: NOT DETECTED",C_WARN,1,32);
    }
    /* Footer stays visible even while status/USB events change. */
    txt(50,670,"UP / DOWN  SELECT",C_TEXT,1,23);
    txt(269,670,"ENTER  LAUNCH",C_ACCENT,1,22);
    txt(446,670,"L/R  SKIP",C_TEXT,1,18);
    txt(609,670,"R  RESCAN",C_MUTED,1,15);
    txt(749,670,"V  VIDEO  /  P  REPORT",C_MUTED,1,32);
    txt(1035,670,"TAB  SYS",C_ACCENT,1,20);
    const char *footer=m->last_message&&*m->last_message?
                   m->last_message:m->last_performance;
    if(footer&&*footer)txt(50,697,footer,C_MUTED,1,125);
    else txt(50,697,"FRF  RGB565  |  720P60  |  ZERO IN-GAME ROM SCANS",
                  C_MUTED,1,65);
    /* A menu frame only becomes visible AFTER every RGB565 pixel is
       drawn. Reuse the old front page solely after a full VBlank-fenced
       hardware page flip. This removes selector redraw tearing. */
    __sync_synchronize();
    if(g_gui_double && g_gui_flip){
        if(g_gui_flip(target)){
            g_gui_front=target;
        }else{
            ++g_gui_failed;
            /* Firmware could not flip; fall back to drawing page 0
               directly rather than repeatedly switching unknown pages. */
            g_gui_double=0u;
            memcpy(g_fb_base,g_pixels,
                   (size_t)g_stride*g_h*sizeof(uint16_t));
            __sync_synchronize();
            (void)g_gui_flip(0u);
            g_gui_front=0;
        }
    }
    g_pixels=g_fb_base;
}
