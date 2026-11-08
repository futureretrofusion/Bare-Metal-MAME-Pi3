/* Optional VideoCore TV service mode enumeration.
   Isolated translation unit: legacy VCHIQ Linux compatibility headers must
   not share a C++ translation unit with MAME/newlib pthread headers. */
#include <vc4/vchiq/vchiqdevice.h>
#include <vc4/vchi/vchi.h>
#include <vc4/interface/vmcs_host/vc_tvservice.h>
#include <circle/memory.h>
#include <circle/interrupt.h>
struct FRFVC4Mode { unsigned width,height,hz,group,code,native,interlaced; };
struct FRFVC4State {unsigned valid,width,height,hz,group,code;};
extern "C" unsigned frf_vc4_enumerate(CInterruptSystem *irq,
                                      void *output, unsigned capacity,
                                      void *display_state)
{
    if(!irq || !output || !capacity)return 0;
    static CVCHIQDevice vchiq(CMemorySystem::Get(),irq);
    if(!vchiq.Initialize())return 0;
    VCHI_INSTANCE_T instance=0;
    VCHI_CONNECTION_T *connection=0;
    if(vchi_initialise(&instance)!=0)return 0;
    if(vchi_connect(nullptr,0,instance)!=0)return 0;
    if(vc_vchi_tv_init(instance,&connection,1)!=0)return 0;
    if(display_state){
        auto *out=(FRFVC4State*)display_state;
        TV_DISPLAY_STATE_T state={};
        if(vc_tv_get_display_state(&state)==0){
            out->valid=1u;
            out->width=state.display.hdmi.width;
            out->height=state.display.hdmi.height;
            out->hz=state.display.hdmi.frame_rate;
            out->group=state.display.hdmi.group;
            out->code=state.display.hdmi.mode;
        }
    }
    static TV_SUPPORTED_MODE_NEW_T raw[128];
    FRFVC4Mode *dest=(FRFVC4Mode*)output;
    unsigned written=0;
    for(unsigned group=1;group<=2;group++){
        HDMI_RES_GROUP_T pg=HDMI_RES_GROUP_INVALID;
        uint32_t preferred=0;
        int count=vc_tv_hdmi_get_supported_modes_new(
            (HDMI_RES_GROUP_T)group,raw,128,&pg,&preferred);
        if(count<=0)continue;
        for(int i=0;i<count && i<128 && written<capacity;i++){
            const TV_SUPPORTED_MODE_NEW_T &m=raw[i];
            dest[written++]={m.width,m.height,m.frame_rate,
                             group,m.code,m.native,m.scan_mode};
        }
    }
    return written;
}
