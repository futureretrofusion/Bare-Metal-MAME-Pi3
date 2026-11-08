/* FRF MAME 0.106 Pi3 TinyUSB DWC2 HID host.
 * TinyUSB core and BCM DWC2 are MIT-licensed. This is a new Circle adapter.
 * Not dependent on Emu68's Amiga CIA, video, mass storage or big-endian MMU.
 * CPU1 owns all DWC2 MMIO and TinyUSB host/event processing.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
extern "C" {
#include "tusb.h"
#include "host/hcd.h"
#include "class/hid/hid_host.h"
void bm_tusb_kbd_input(uint8_t modifier,const uint8_t keys[6]);
void bm_tusb_pad_input(uint32_t mask);
uint64_t bm_cycles(void);
}
#include "../bm_platform.h"
#include "frf_hid_gamepad.h"
#include "frf_known_pads.h"
#include <circle/bcmpropertytags.h>

static volatile uint32_t g_init_ok=0,g_device_mount=0,g_keyboard_mount=0,g_pad_mount=0;
static volatile uint32_t g_enum_ok=0,g_enum_errors=0;
/* Circle's USB initialization was bypassed to avoid sharing DWC2 with
   TinyUSB. This also bypassed its mandatory firmware USB power request.
   Follow the known-good Circle/EMU68AGA startup sequence explicitly. */
static volatile uint32_t g_usb_power_ok=0,g_usb_power_state=0;
static volatile uint32_t g_dwc2_id=0,g_dwc2_hprt=0;
extern "C" void bm_tusb_hw_stats(uint32_t out[7]) {
    if(!out)return;
    out[0]=g_usb_power_ok;
    out[1]=g_usb_power_state;
    out[2]=g_dwc2_id;
    out[3]=g_dwc2_hprt;
    out[4]=g_device_mount;
    out[5]=g_enum_ok;
    out[6]=g_enum_errors;
}
static bool frf_usb_power_on() {
    CBcmPropertyTags Tags;
    TPropertyTagPowerState PowerState = {};
    PowerState.nDeviceId = DEVICE_ID_USB_HCD;
    PowerState.nState = POWER_STATE_ON | POWER_STATE_WAIT;
    boolean rc = Tags.GetTag(PROPTAG_SET_POWER_STATE,
                              &PowerState,sizeof PowerState);
    g_usb_power_state = PowerState.nState;
    g_usb_power_ok = rc && !(PowerState.nState & POWER_STATE_NO_DEVICE)
                         && (PowerState.nState & POWER_STATE_ON);
    return g_usb_power_ok!=0;
}

/* Emu68-specific bus diagnostics are replaced by local counters; the
   receiver-specialized NAK workaround is disabled unless its addr is known. */
extern "C" {
volatile uint8_t frf_r26_receiver_addr=0;
volatile uint8_t frf_r32_receiver_addr=0;
void frf_pi3_trace_control_length(uint16_t,uint16_t) {}
void frf_pi3_trace_ep(uint32_t,uint32_t,uint32_t) {}
void frf_pi3_trace_xfer(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t) {}
void frf_tinyusb_enum_observe(uint32_t,uint32_t);
}

extern "C" void frf_tinyusb_enum_observe(uint32_t,uint32_t result){
    if(result==0)++g_enum_ok;else ++g_enum_errors;
}

static volatile uint32_t g_kbd_reports=0,g_pad_reports=0,g_rx_retries=0,g_usb_errors=0;
static uint8_t g_keyboard_addr=0,g_keyboard_inst=0;
static uint8_t g_pad_addr=0,g_pad_inst=0;
static uint32_t g_last_kbd_ms=0,g_last_pad_ms=0;
static unsigned g_report_len=0;
static uint8_t g_report_bytes[12];
static unsigned g_report_id=0;
static volatile uint32_t g_vidpid=0;
static volatile uint32_t g_pad_vidpid=0,g_last_pad_mask=0;
/* 1 generic descriptor; 2 legacy; 3 Switch Pro; 4 ShanWan */
static volatile uint32_t g_pad_decode_mode=0;
static uint8_t g_switch_stage=0,g_switch_attempts=0;
static uint32_t g_switch_last_cmd_ms=0;
static uint8_t g_hid_mount_addr=0,g_hid_mount_inst=0,g_hid_mount_proto=0;
static volatile uint32_t g_hid_mount_generation=0;
static volatile uint32_t g_hid_mount_vidpid=0,g_hid_desc_len=0,g_hid_claimed=0;
extern "C" void bm_tusb_hid_mount_info(uint32_t out[8]){
    if(!out)return;
    out[0]=g_hid_mount_generation;out[1]=g_hid_mount_vidpid;
    out[2]=g_hid_mount_addr;out[3]=g_hid_mount_inst;
    out[4]=g_hid_mount_proto;out[5]=g_hid_desc_len;
    out[6]=g_hid_claimed;out[7]=g_switch_stage;
}
static frf_hid::Layout g_pad_layout={};
extern "C" void bm_tusb_pad_detail(uint32_t out[12]){
    if(!out)return;
    out[0]=g_pad_addr?1u:0u;
    out[1]=g_pad_vidpid;
    out[2]=g_pad_reports;
    out[3]=g_last_pad_mask;
    out[4]=g_report_id;
    out[5]=g_report_len;
    uint32_t raw0=0,raw1=0;
    for(unsigned i=0;i<4;i++)raw0|=(uint32_t)g_report_bytes[i]<<(i*8);
    for(unsigned i=0;i<4;i++)raw1|=(uint32_t)g_report_bytes[i+4]<<(i*8);
    out[6]=raw0;out[7]=raw1;
    out[8]=g_pad_decode_mode;
    out[9]=g_usb_errors;
    out[10]=g_pad_layout.x.active?g_pad_layout.x.offset:0xffffu;
    out[11]=g_pad_layout.hat.active?g_pad_layout.hat.offset:0xffffu;
}
extern "C" void bm_tusb_last_report(uint32_t out[5]){
    if(!out)return;
    out[0]=g_vidpid;out[1]=g_report_len;
    uint32_t w0=0,w1=0;
    for(unsigned i=0;i<4;i++) w0|=(uint32_t)g_report_bytes[i]<<(8*i);
    for(unsigned i=0;i<4;i++) w1|=(uint32_t)g_report_bytes[i+4]<<(8*i);
    out[2]=w0;out[3]=w1;out[4]=g_report_id;
}
static uint32_t now_ms(void){return (uint32_t)(bm_cycles()/1000u);}
extern "C" uint32_t tusb_time_millis_api(void){return now_ms();}
extern "C" void *tusb_app_virt_to_phys(void *ptr){
    uintptr_t addr=(uintptr_t)ptr;
    /* Circle Pi3 ARM32 maps normal RAM identity. DWC2 DMA reads via GPU
       0xC0000000 bus alias. Reject peripheral addresses. */
    if(!ptr || addr>=0x40000000u) {++g_usb_errors;return 0;}
    return (void*)(uintptr_t)((addr & 0x3fffffffu)|0xc0000000u);
}
extern "C" void arm_flush_cache(uintptr_t addr,uint32_t size){
    if(!size) return;
    uintptr_t end=addr+size;
    for(uintptr_t p=addr & ~(uintptr_t)63u;p<end;p+=64u) {
        uint32_t x=(uint32_t)p;
        asm volatile("mcr p15,0,%0,c7,c10,1"::"r"(x):"memory");
    }
    asm volatile("dsb sy":::"memory");
}
extern "C" void arm_dcache_invalidate(uintptr_t addr,uint32_t size){
    if(!size) return;
    uintptr_t end=addr+size;
    for(uintptr_t p=addr & ~(uintptr_t)63u;p<end;p+=64u) {
        uint32_t x=(uint32_t)p;
        asm volatile("mcr p15,0,%0,c7,c6,1"::"r"(x):"memory");
    }
    asm volatile("dsb sy":::"memory");
}
extern "C" void tusb_app_dcache_flush(uintptr_t a,uint32_t n){arm_flush_cache(a,n);}
extern "C" void tusb_app_dcache_invalidate(uintptr_t a,uint32_t n){arm_dcache_invalidate(a,n);}
extern "C" void tu_hwfifo_write(volatile void *fifo,const uint8_t *src,uint16_t n,const tu_hwfifo_access_t*){
    for(unsigned i=0;i<n;i+=4u){
        uint32_t w=0;
        for(unsigned j=0;j<4u&&i+j<n;j++)w|=(uint32_t)src[i+j]<<(8*j);
        *(volatile uint32_t*)fifo=w; /* ARM32 little endian */
    }
}
extern "C" void tu_hwfifo_read(const volatile void *fifo,uint8_t *dst,uint16_t n,const tu_hwfifo_access_t*){
    for(unsigned i=0;i<n;i+=4u){
        uint32_t w=*(const volatile uint32_t*)fifo;
        for(unsigned j=0;j<4u&&i+j<n;j++)dst[i+j]=(uint8_t)(w>>(8*j));
    }
}
static void frf_switch_init_tick(){
    if(g_pad_decode_mode!=3u || !g_pad_addr || !g_switch_stage)return;
    const uint32_t now=now_ms();
    if((uint32_t)(now-g_switch_last_cmd_ms)<130u)return;
    if(g_switch_stage>5u){
        if(g_switch_stage==6u && g_pad_reports==0u &&
           (uint32_t)(now-g_switch_last_cmd_ms)>3000u &&
           g_switch_attempts<2u){
            ++g_switch_attempts;
            g_switch_stage=1;
        }
        return;
    }
    bool accepted=false;
    if(g_switch_stage<=4u){
        const uint8_t cmd=(g_switch_stage==1u||g_switch_stage==3u)?
            0x02u:(g_switch_stage==2u?0x03u:0x04u);
        /* TinyUSB prepends output Report ID 0x80. Full USB
           command is 0x80,cmd. No blocking or host reset. */
        accepted=tuh_hid_send_report(g_pad_addr,g_pad_inst,0x80,&cmd,1);
    }else{
        /* Report 0x01: 1 packet number + 8 neutral rumble bytes,
           then subcommand 0x03 and argument 0x30 (full reports). */
        const uint8_t pkt[11]={0,0,1,0x40,0x40,0,1,0x40,0x40,0x03,0x30};
        accepted=tuh_hid_send_report(g_pad_addr,g_pad_inst,0x01,pkt,sizeof(pkt));
    }
    if(accepted){
        ++g_switch_stage;
        g_switch_last_cmd_ms=now;
    }else{
        /* TinyUSB endpoint may be busy: retry bounded every 130ms.
           The HID keyboard continues without interference. */
        g_switch_last_cmd_ms=now;
    }
}
static bool report_next(uint8_t addr,uint8_t inst){
    if(tuh_hid_receive_report(addr,inst))return true;
    ++g_usb_errors;return false;
}
/* USB device-level discovery also captures vendor-specific XInput-type
 * controllers, which do NOT invoke TinyUSB HID mount callbacks. This
 * is diagnostics only: never claim they are a working gamepad yet. */
struct FrfUsbDevice {
    uint16_t vid,pid;
    uint8_t addr,active;
};
static FrfUsbDevice g_usb_devices[16]={};
static volatile uint32_t g_usb_device_generation=0;
static volatile uint32_t g_usb_last_vidpid=0;
extern "C" void bm_tusb_usb_device_info(uint32_t out[8]){
    if(!out)return;
    /* Prefer a non-hub, non-keyboard device so the Pi3's internal
       LAN9514 hub does not mask a connected vendor-class controller. */
    uint32_t candidate=0,selected_addr=0,selected_weight=0;
    unsigned count=0;
    for(unsigned i=0;i<16;i++){
        const FrfUsbDevice &d=g_usb_devices[i];
        if(!d.active)continue;
        ++count;
        unsigned weight=(d.addr==g_pad_addr?5u:
                         d.addr==g_keyboard_addr?1u:
                         (d.vid==0x0424 && (d.pid==0x9514 ||
                                          d.pid==0x2514 || d.pid==0xec00))?0u:3u);
        if(weight>=selected_weight && weight){
            candidate=((uint32_t)d.vid<<16)|d.pid;
            selected_addr=d.addr;selected_weight=weight;
        }
    }
    out[0]=candidate;out[1]=selected_addr;out[2]=count;
    out[3]=g_usb_device_generation;
    out[4]=g_usb_last_vidpid;
    out[5]=g_keyboard_addr;out[6]=g_pad_addr;
    out[7]=g_enum_errors;
}
extern "C" void tuh_mount_cb(uint8_t addr){
    uint16_t vid=0,pid=0;
    tuh_vid_pid_get(addr,&vid,&pid);
    if(addr<16){
        g_usb_devices[addr]={vid,pid,addr,1};
    }
    ++g_usb_device_generation;
    g_usb_last_vidpid=((uint32_t)vid<<16)|pid;
    ++g_device_mount;
}
extern "C" void tuh_umount_cb(uint8_t addr){
    if(addr<16)g_usb_devices[addr].active=0;
    ++g_usb_device_generation;
    if(g_device_mount)--g_device_mount;
}
extern "C" void tuh_hid_mount_cb(uint8_t addr,uint8_t inst,
                                  uint8_t const* desc,uint16_t len) {
    uint16_t vid=0,pid=0;
    tuh_vid_pid_get(addr,&vid,&pid);
    g_vidpid=((uint32_t)vid<<16)|pid;
    auto proto=tuh_hid_interface_protocol(addr,inst);
    const bool switch_pro=(vid==0x057eu && pid==0x2009u);
    const bool shanwan=(vid==0x2563u && (pid==0x0576u || pid==0x0575u));
    ++g_hid_mount_generation;
    g_hid_mount_vidpid=((uint32_t)vid<<16)|pid;
    g_hid_mount_addr=addr;g_hid_mount_inst=inst;
    g_hid_mount_proto=(uint8_t)proto;g_hid_desc_len=len;
    g_hid_claimed=0;
    /* Never misclassify a known controller as a keyboard, even if its
       composite HID interface advertises a keyboard subclass. */
    if(proto==HID_ITF_PROTOCOL_KEYBOARD && !switch_pro && !shanwan){
        g_keyboard_addr=addr;g_keyboard_inst=inst;++g_keyboard_mount;
        const uint8_t zeros[6]={0};
        bm_tusb_kbd_input(0,zeros);
    }else if(switch_pro || shanwan || (len && desc)){
        /* Try descriptor-driven decoding rather than fixed byte offsets.
           Handles HID joystick, gamepad, report IDs, axes, hats, buttons. */
        frf_hid::Layout layout={};
        bool mapped=frf_hid::parse(desc,len,&layout);
        bool ispad=mapped||switch_pro||shanwan;
        uint8_t reportid=mapped?layout.report_id:0u;
        if(!ispad){
            tuh_hid_report_info_t infos[8];
            uint8_t n=tuh_hid_parse_report_descriptor(infos,8,desc,len);
            for(uint8_t i=0;i<n;i++){
                if(infos[i].usage_page==HID_USAGE_PAGE_DESKTOP &&
                  (infos[i].usage==HID_USAGE_DESKTOP_GAMEPAD ||
                   infos[i].usage==HID_USAGE_DESKTOP_JOYSTICK)){
                    ispad=true;reportid=infos[i].report_id;break;
                }
            }
        }
        if(ispad && !g_pad_addr){
            g_pad_addr=addr;g_pad_inst=inst;g_report_id=reportid;
            g_pad_layout=layout;
            g_pad_decode_mode=switch_pro?3u:shanwan?4u:mapped?1u:2u;
            g_hid_claimed=g_pad_decode_mode;
            g_switch_stage=switch_pro?1u:0u;
            g_switch_attempts=0;
            g_switch_last_cmd_ms=now_ms();
            g_pad_vidpid=((uint32_t)vid<<16)|(uint32_t)pid;
            g_pad_mount=1;
            g_pad_reports=0;g_last_pad_mask=0;g_report_len=0;
            memset(g_report_bytes,0,sizeof(g_report_bytes));
            bm_tusb_pad_input(0);
        }
    }
    report_next(addr,inst);
}
extern "C" void tuh_hid_umount_cb(uint8_t addr,uint8_t inst){
    if(g_keyboard_addr==addr && g_keyboard_inst==inst){
        g_keyboard_addr=0;const uint8_t zeros[6]={0};
        bm_tusb_kbd_input(0,zeros);
    }
    if(g_pad_addr==addr && g_pad_inst==inst){
        g_pad_addr=0;g_pad_mount=0;g_pad_decode_mode=0;
        g_switch_stage=0;
        g_last_pad_mask=0;
        memset(&g_pad_layout,0,sizeof(g_pad_layout));
        bm_tusb_pad_input(0);
    }
}
static uint32_t decode_gamepad(const uint8_t *r,unsigned len){
    if(len<2) return 0;
    uint32_t m=0;
    /* Generic HID XY axes. Common 0..255, center 128. */
    if(r[0]<70) m|=BM_PAD_LEFT;
    if(r[0]>185) m|=BM_PAD_RIGHT;
    if(r[1]<70) m|=BM_PAD_UP;
    if(r[1]>185) m|=BM_PAD_DOWN;
    if(len>=5){
        unsigned hat=r[4]&0x0fu;
        if(hat==0||hat==1||hat==7)m|=BM_PAD_UP;
        if(hat==1||hat==2||hat==3)m|=BM_PAD_RIGHT;
        if(hat==3||hat==4||hat==5)m|=BM_PAD_DOWN;
        if(hat==5||hat==6||hat==7)m|=BM_PAD_LEFT;
        uint32_t buttons=(r[4]>>4);
        if(len>=6)buttons|=(uint32_t)r[5]<<4;
        if(len>=7)buttons|=(uint32_t)r[6]<<12;
        if(buttons&(1u<<0))m|=BM_PAD_B1;
        if(buttons&(1u<<1))m|=BM_PAD_B2;
        if(buttons&(1u<<2))m|=BM_PAD_B3;
        if(buttons&(1u<<3))m|=BM_PAD_B4;
        if(buttons&(1u<<4))m|=BM_PAD_B5;
        if(buttons&(1u<<5))m|=BM_PAD_B6;
        if(buttons&(1u<<6))m|=BM_PAD_B7;
        if(buttons&(1u<<7))m|=BM_PAD_B8;
        if(buttons&(1u<<8))m|=BM_PAD_SELECT;
        if(buttons&(1u<<9))m|=BM_PAD_START;
    } else if(len>=3) {
        if(r[2]&1u)m|=BM_PAD_B1;
        if(r[2]&2u)m|=BM_PAD_B2;
    }
    return m;
}
extern "C" void tuh_hid_report_received_cb(uint8_t addr,uint8_t inst,
                                             uint8_t const*data,uint16_t len){
    const uint32_t t=now_ms();
    if(addr==g_keyboard_addr && inst==g_keyboard_inst &&
       len>=sizeof(hid_keyboard_report_t)){
        auto *k=(const hid_keyboard_report_t*)data;
        bm_tusb_kbd_input(k->modifier,k->keycode);
        ++g_kbd_reports;g_last_kbd_ms=t;
    } else if(addr==g_pad_addr && inst==g_pad_inst){
        uint8_t const*r=data;
        unsigned n=len;
        uint32_t mask=0;
        if(g_pad_decode_mode==3u){
            /* Known 057e:2009 Switch Pro, or descriptor fallback for
               0x3F legacy report. Ignore USB handshake replies 0x81. */
            if(!frf_pads::switch_pro(r,n,&mask)){
                if(g_pad_layout.valid && n && r[0]==g_pad_layout.report_id &&
                   n*8u>=g_pad_layout.report_bits)
                    mask=frf_hid::decode(g_pad_layout,r,n);
                else goto next_report;
            }
        }else if(g_pad_decode_mode==4u){
            /* Established Dyonder/ShanWan ID1/10-byte packet. */
            if(!frf_pads::shanwan(r,n,&mask))goto next_report;
        }else if(g_pad_decode_mode==1u){
            /* Report ID may multiplex several app collections. Ignore
               unselected reports instead of clearing pressed buttons. */
            if(g_pad_layout.report_id &&
               (!n || r[0]!=g_pad_layout.report_id))goto next_report;
            mask=frf_hid::decode(g_pad_layout,r,n);
        }else{
            /* Support old known layouts when descriptor parsing fails. */
            if(g_report_id){
                if(!n || r[0]!=g_report_id)goto next_report;
                ++r;--n;
            }
            mask=decode_gamepad(r,n);
        }
        g_last_pad_mask=mask;
        bm_tusb_pad_input(mask);
        ++g_pad_reports;g_last_pad_ms=t;
        g_report_len=len;
        memset(g_report_bytes,0,sizeof(g_report_bytes));
        memcpy(g_report_bytes,data,len<sizeof(g_report_bytes)?len:sizeof(g_report_bytes));
    }
next_report:
    if(tuh_hid_receive_ready(addr,inst))report_next(addr,inst);
}
extern "C" void bm_tusb_stats(uint32_t out[7]){
    out[0]=g_init_ok;out[1]=g_keyboard_mount;out[2]=g_pad_mount;
    out[3]=g_kbd_reports;out[4]=g_pad_reports;out[5]=g_usb_errors;
    out[6]=g_rx_retries;
}
extern "C" void bm_tusb_host_cpu1_loop(void){
    /* Only the Pi3 DWC2 is initialized. Circle mUSBHCI is disabled in
       the exclusive TinyUSB kernel's early initialization. */
    bool power_ok=frf_usb_power_on();
    /* Read real Synopsys core-ID: avoids claiming host success if the
       controller is still unpowered or MMIO is wrong. */
    g_dwc2_id=*(volatile uint32_t*)(uintptr_t)(0x3f980000u+0x040u);
    if(!power_ok && (g_dwc2_id & 0xffff0000u)!=0x4f540000u) {
        ++g_usb_errors;
        for(;;) asm volatile("wfe");
    }
    /* USB firmware power-on needs settling before PHY reset/enumeration. */
    const uint32_t power_start=now_ms();
    while((uint32_t)(now_ms()-power_start)<100u)asm volatile("yield");
    const tusb_rhport_init_t cfg={TUSB_ROLE_HOST,TUSB_SPEED_HIGH};
    g_init_ok=tuh_rhport_init(0,&cfg)?1u:0u;
    g_dwc2_hprt=*(volatile uint32_t*)(uintptr_t)(0x3f980000u+0x440u);
    if(!g_init_ok)for(;;)asm volatile("wfe");
    bool was=hcd_port_connect_status(0);
    if(was) hcd_event_device_attach(0,false); /* internal LAN9514 hub */
    uint32_t last_enum=now_ms(),last_watch=0;
    for(;;){
        hcd_int_handler(0,false);
        tuh_task_ext(0,false);
        uint32_t now=now_ms();
        if((uint32_t)(now-last_watch)>=25){
            g_dwc2_hprt=*(volatile uint32_t*)(uintptr_t)(0x3f980000u+0x440u);
            last_watch=now;
            frf_switch_init_tick();
            if(g_keyboard_addr && tuh_hid_receive_ready(g_keyboard_addr,g_keyboard_inst)){
                ++g_rx_retries;report_next(g_keyboard_addr,g_keyboard_inst);
            }
            if(g_pad_addr && tuh_hid_receive_ready(g_pad_addr,g_pad_inst)){
                ++g_rx_retries;report_next(g_pad_addr,g_pad_inst);
            }
        }
        bool connected=hcd_port_connect_status(0);
        if(!was && connected)hcd_event_device_attach(0,false);
        if(connected && !g_device_mount && (uint32_t)(now-last_enum)>1500u){
            hcd_event_device_attach(0,false);
            last_enum=now;
        }
        was=connected;
        asm volatile("yield":::"memory");
    }
}
