#include "bm_platform.h"
#include <circle/timer.h>

extern "C" uint64_t bm_cycles(void)
{
    return (uint64_t) CTimer::GetClockTicks64();
}

extern "C" uint64_t bm_cycles_per_second(void)
{
    return 1000000ULL;
}
