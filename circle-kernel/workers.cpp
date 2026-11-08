#include "workers.h"
#ifdef FRF_BM_ASYNC_RENDER
extern "C" void bm_async_render_worker_loop(void);
#endif
#ifdef FRF_BM_ASYNC_VSYNC
extern "C" void bm_flip_worker_loop(void);
#endif
#ifdef FRF_TINYUSB_MAME_HOST
extern "C" void bm_tusb_host_cpu1_loop(void);
#endif
extern "C" {
#include "../baremetal/bm_platform.h"
}

CMameWorkers::CMameWorkers(CMemorySystem *pMemorySystem)
: CMultiCoreSupport(pMemorySystem)
{
}

void CMameWorkers::Run(unsigned nCore)
{
    if (nCore == 0)
        return;
#ifdef FRF_TINYUSB_MAME_HOST
    if(nCore==1) {
        bm_tusb_host_cpu1_loop();
    }
#endif

#ifdef FRF_BM_ASYNC_RENDER
    if(nCore==2) bm_async_render_worker_loop();
#endif
#ifdef FRF_BM_ASYNC_VSYNC
    if(nCore==2) bm_flip_worker_loop();
#endif
    /* FRF v0.46: CPU0 exclusively renders/presents frames.
       Prior code: CPU0 rendered & copied each frame and also signalled
       CPU1 and CPU2 to repeat the same work on shared g_stage/g_fb.
       This caused unsynchronized writes, flicker, and bus contention.
       Leave auxiliary cores parked until a queue/fence based pipeline
       has been proven on Pi3 hardware. */
    (void)nCore;

    for (;;)
        asm volatile("wfe");
}
