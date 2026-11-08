/* FRF v1.14: proven ShanWan/Dyonder ID1 packet and Switch Pro report 0x30
 * decoders. Header remains independent of USB hardware for native tests. */
#ifndef FRF_KNOWN_PADS_H
#define FRF_KNOWN_PADS_H
#include <stdint.h>
#include <stddef.h>
namespace frf_pads {
enum { LEFT=1u<<0, RIGHT=1u<<1, UP=1u<<2, DOWN=1u<<3,
       B1=1u<<4, B2=1u<<5, START=1u<<6, SELECT=1u<<7,
       B3=1u<<8,B4=1u<<9,B5=1u<<10,B6=1u<<11 };
static inline uint32_t hat8(unsigned h){
    uint32_t m=0;
    if(h>7u)return 0;
    if(h==0||h==1||h==7)m|=UP;
    if(h==1||h==2||h==3)m|=RIGHT;
    if(h==3||h==4||h==5)m|=DOWN;
    if(h==5||h==6||h==7)m|=LEFT;
    return m;
}
static inline uint32_t buttons(unsigned b){
    uint32_t m=0;
    if(b&(1u<<0))m|=B1;
    if(b&(1u<<1))m|=B2;
    if(b&(1u<<2))m|=B3;
    if(b&(1u<<3))m|=B4;
    if(b&(1u<<4))m|=B5;
    if(b&(1u<<5))m|=B6;
    if(b&(1u<<8))m|=SELECT;
    if(b&(1u<<9))m|=START;
    return m;
}
/* Ported from known physical 2563:0576 report layout in our EMU68AGA
 * USPi usbgamepad.c. ID1 report is exactly 10 bytes including ID.
 * Do not interpret ID5 mouse as a gamepad. */
static inline bool shanwan(const uint8_t*r,size_t n,uint32_t *out){
    if(!out||!r||n!=10 || r[0]!=1)return false;
    uint32_t m=buttons((unsigned)r[1]|((unsigned)(r[2]&0x7f)<<8));
    m|=hat8(r[3]&0x0f);
    if(r[4]<69)m|=LEFT;
    if(r[4]>186)m|=RIGHT;
    if(r[5]<69)m|=UP;
    if(r[5]>186)m|=DOWN;
    *out=m;
    return true;
}
/* Nintendo 057E:2009 standard USB input report 0x30:
 * [3]=face/right shoulder, [4]=+/-, [5]=D-pad/left shoulder,
 * [6..8]=packed 12-bit left stick. 0x21 subcommand response contains
 * same leading button/stick state. Other report IDs need descriptor
 * decoding instead and should not clear held button state. */
static inline bool switch_pro(const uint8_t*r,size_t n,uint32_t*out){
    if(!out||!r||n<12 || (r[0]!=0x30 && r[0]!=0x21))return false;
    uint32_t m=0;
    if(r[5]&0x02)m|=UP;
    if(r[5]&0x01)m|=DOWN;
    if(r[5]&0x08)m|=LEFT;
    if(r[5]&0x04)m|=RIGHT;
    if(r[3]&0x04)m|=B1; /* Nintendo B: primary fire */
    if(r[3]&0x08)m|=B2; /* A: jump */
    if(r[3]&0x01)m|=B3; /* Y */
    if(r[3]&0x02)m|=B4; /* X */
    if(r[3]&0x40)m|=B5;
    if(r[5]&0x40)m|=B6;
    if(r[4]&0x02)m|=START;
    if(r[4]&0x01)m|=SELECT;
    unsigned x=(unsigned)r[6] | (((unsigned)r[7]&0x0f)<<8);
    unsigned y=((unsigned)r[7]>>4)|((unsigned)r[8]<<4);
    if(x<1350u)m|=LEFT;
    if(x>2700u)m|=RIGHT;
    if(y<1350u)m|=DOWN;
    if(y>2700u)m|=UP;
    *out=m;
    return true;
}
}
#endif
