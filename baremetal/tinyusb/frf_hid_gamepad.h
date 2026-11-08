/* Future Retro Fusion: generic USB HID gamepad INPUT-report decoder.
 * No TinyUSB, Circle or MAME dependency; suitable for host unit tests.
 * Parse HID 1.11 short items, report IDs, variable/array button fields,
 * signed/unsigned X/Y axes and hat switch. No heap allocations.
 * 2026-11-07
 */
#ifndef FRF_HID_GAMEPAD_H
#define FRF_HID_GAMEPAD_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace frf_hid {
struct Field {
    uint16_t offset;
    uint8_t size;
    uint8_t active;
    int32_t min, max;
};
struct Layout {
    Field x,y,hat,dpad[4],buttons[16],button_array[16];
    uint8_t report_id,valid,buttons_seen,arrays_seen;
    uint16_t report_bits;
};
static inline int32_t sign_value(uint32_t v,unsigned bits){
    if(bits==0||bits>=32)return (int32_t)v;
    const uint32_t bit=1u<<(bits-1u);
    return (int32_t)((v^bit)-bit);
}
static inline uint32_t read_bits(const uint8_t *report,size_t length,Field f){
    if(!f.active || f.size==0 || f.size>32 || (size_t)f.offset+f.size>length*8u)return 0;
    uint32_t v=0;
    for(unsigned j=0;j<f.size;j++){
        unsigned at=f.offset+j;
        if(report[at/8u] & (uint8_t)(1u<<(at%8u)))v|=1u<<j;
    }
    return v;
}
static inline int32_t value(const uint8_t *r,size_t n,Field f){
    uint32_t v=read_bits(r,n,f);
    return f.min<0?sign_value(v,f.size):(int32_t)v;
}
static inline bool in_bounds(Field f,size_t n){
    return f.active && f.size>0 && f.size<=32 &&
           (size_t)f.offset+(size_t)f.size<=n*8u;
}
static inline uint32_t choose_button(unsigned n){
    static const uint32_t flags[16]={
        1u<<4,1u<<5,1u<<8,1u<<9,1u<<10,1u<<11,
        1u<<12,1u<<13,1u<<7,1u<<6,1u<<14,1u<<15,0,0,0,0};
    return n<16?flags[n]:0;
}
static inline bool parse(const uint8_t *d,size_t n,Layout *out){
    if(!out)return false;
    memset(out,0,sizeof(*out));
    if(!d||!n)return false;
    uint32_t page=0,report_size=0,report_count=0,report_id=0;
    int32_t logical_min=0,logical_max=255;
    struct Global{
        uint32_t page,size,count,id;
        int32_t minimum,maximum;
    }stack[4];
    unsigned sp=0;
    uint16_t offsets[256]={};
    uint32_t usages[24]={};
    unsigned usage_count=0;
    uint32_t range_min=0,range_max=0;
    bool has_min=false,has_max=false;
    unsigned collection_depth=0,pad_depth=0;
    for(size_t i=0;i<n;){
        uint8_t prefix=d[i++];
        if(prefix==0xfe){
            if(i+2>n)return false;
            unsigned size=d[i];i+=2;
            if(i+size>n)return false;
            i+=size;continue;
        }
        unsigned size_code=prefix&3u;
        unsigned length=size_code==3?4:size_code;
        unsigned type=(prefix>>2)&3u;
        unsigned tag=(prefix>>4)&15u;
        if(i+length>n)return false;
        uint32_t raw=0;
        for(unsigned j=0;j<length;j++)raw|=(uint32_t)d[i+j]<<(8*j);
        i+=length;
        int32_t signed_raw=sign_value(raw,length*8u);
        if(type==1){
            switch(tag){
                case 0:page=raw;break;
                case 1:logical_min=signed_raw;break;
                case 2:logical_max=logical_min<0?signed_raw:(int32_t)raw;break;
                case 7:report_size=raw;break;
                case 8:report_id=raw&255u;break;
                case 9:report_count=raw;break;
                case 10:
                    if(sp<4)stack[sp++]={page,report_size,report_count,report_id,
                                          logical_min,logical_max};
                    break;
                case 11:
                    if(sp){const Global &g=stack[--sp];
                        page=g.page;report_size=g.size;report_count=g.count;
                        report_id=g.id;logical_min=g.minimum;logical_max=g.maximum;}
                    break;
            }
        }else if(type==2){
            uint32_t full=(length==4)?raw:((page<<16)|raw);
            if(tag==0 && usage_count<24)usages[usage_count++]=full;
            if(tag==1){range_min=full;has_min=true;}
            if(tag==2){range_max=full;has_max=true;}
        }else if(type==0){
            if(tag==10){ /* Collection */
                uint32_t u=usage_count?usages[0]:(has_min?range_min:0u);
                collection_depth++;
                if(!pad_depth && (u>>16)==0x01 &&
                   ((u&0xffff)==0x04 || (u&0xffff)==0x05 || (u&0xffff)==0x08))
                    pad_depth=collection_depth;
            }else if(tag==12){ /* End Collection */
                if(pad_depth==collection_depth)pad_depth=0;
                if(collection_depth)collection_depth--;
            }else if(tag==8){ /* Input */
                unsigned rid=report_id&255u;
                unsigned count=report_count<128u?report_count:128u;
                unsigned bits=report_size<33u?report_size:0u;
                unsigned long_width=(unsigned long)count*report_size;
                uint32_t off=offsets[rid];
                bool applies=pad_depth && bits && !(raw&1u);
                if(applies && (!out->valid || out->report_id==rid)){
                    if(!out->valid){out->valid=1;out->report_id=(uint8_t)rid;}
                    bool is_var=(raw&2u)!=0;
                    for(unsigned k=0;k<count;k++){
                        uint32_t usage=0;
                        if(k<usage_count)usage=usages[k];
                        else if(has_min && (!has_max || range_min+k<=range_max))
                            usage=range_min+k;
                        else if(usage_count)usage=usages[usage_count-1];
                        uint32_t upage=(usage>>16);
                        uint32_t uid=(usage&0xffffu);
                        Field field={(uint16_t)(off+k*bits),(uint8_t)bits,1,
                                     logical_min,logical_max};
                        if(upage==1){
                            if(uid==0x30 && !out->x.active)out->x=field;
                            if(uid==0x31 && !out->y.active)out->y=field;
                            if(uid==0x39 && !out->hat.active)out->hat=field;
                            if(uid>=0x90 && uid<=0x93 && !out->dpad[uid-0x90].active)
                                out->dpad[uid-0x90]=field;
                        }else if(upage==9){
                            if(is_var && uid>=1 && uid<=16){
                                out->buttons[uid-1]=field;
                                if(out->buttons_seen<uid)out->buttons_seen=(uint8_t)uid;
                            }else if(!is_var && out->arrays_seen<16){
                                out->button_array[out->arrays_seen++]=field;
                            }
                        }
                    }
                }
                if(long_width<0xffffu && off+long_width<=0xffffu)
                    offsets[rid]=(uint16_t)(off+long_width);
                else return false;
                if(out->valid && out->report_id==rid)
                    out->report_bits=offsets[rid];
            }else if(tag==9){ /* Output - report offsets for input unaffected */
            }else if(tag==11){ /* Feature */
            }
            /* Local state resets after every Main item (Input/Collection/End). */
            usage_count=0;has_min=has_max=false;range_min=range_max=0;
        }
    }
    if(out->valid && (!out->x.active&&!out->y.active&&!out->hat.active &&
                     !out->dpad[0].active&&!out->dpad[1].active &&
                     !out->dpad[2].active&&!out->dpad[3].active &&
                     !out->buttons_seen&&!out->arrays_seen))
        out->valid=0;
    return out->valid!=0;
}
static inline uint32_t decode(const Layout &layout,const uint8_t *report,size_t n){
    if(!layout.valid||!report||!n)return 0;
    const uint8_t *r=report;
    /* Reject truncated input reports instead of converting partial bytes into false button presses. */
    if(layout.report_id){
        if(report[0]!=layout.report_id || n<2)return 0;
        ++r;--n;
    }
    if(n*8u<layout.report_bits)return 0;
    uint32_t out=0;
    if(in_bounds(layout.x,n) && layout.x.max>layout.x.min){
        int32_t v=value(r,n,layout.x);
        int32_t middle=layout.x.min+(layout.x.max-layout.x.min)/2;
        int32_t dead=(layout.x.max-layout.x.min)/5;
        if(v<middle-dead)out|=1u<<0;
        if(v>middle+dead)out|=1u<<1;
    }
    if(in_bounds(layout.y,n) && layout.y.max>layout.y.min){
        int32_t v=value(r,n,layout.y);
        int32_t middle=layout.y.min+(layout.y.max-layout.y.min)/2;
        int32_t dead=(layout.y.max-layout.y.min)/5;
        if(v<middle-dead)out|=1u<<2;
        if(v>middle+dead)out|=1u<<3;
    }
    if(in_bounds(layout.hat,n)){
        int32_t h=value(r,n,layout.hat)-layout.hat.min;
        if(h>=0 && h<8){
            if(layout.hat.max-layout.hat.min==3){
                /* HID four-position hat: N, E, S, W. */
                if(h==0)out|=1u<<2;
                if(h==1)out|=1u<<1;
                if(h==2)out|=1u<<3;
                if(h==3)out|=1u<<0;
            }else{
                if(h==0||h==1||h==7)out|=1u<<2;
                if(h==1||h==2||h==3)out|=1u<<1;
                if(h==3||h==4||h==5)out|=1u<<3;
                if(h==5||h==6||h==7)out|=1u<<0;
            }
        }
    }
    for(unsigned k=0;k<4;k++){
        Field f=layout.dpad[k];
        if(in_bounds(f,n) && value(r,n,f))
            out|=(k==0?1u<<2:k==1?1u<<3:k==2?1u<<1:1u<<0);
    }
    for(unsigned k=0;k<16;k++){
        Field f=layout.buttons[k];
        if(in_bounds(f,n) && value(r,n,f))out|=choose_button(k);
    }
    for(unsigned k=0;k<layout.arrays_seen;k++){
        Field f=layout.button_array[k];
        if(!in_bounds(f,n))continue;
        int32_t b=value(r,n,f);
        if(b>=1&&b<=16)out|=choose_button((unsigned)b-1u);
    }
    return out;
}
} // namespace frf_hid
#endif
