#include "bm_platform.h"
uint64_t bm_cycles(void){ uint64_t v; __asm__ volatile("mrs %0,cntvct_el0":"=r"(v)); return v; }
uint64_t bm_cycles_per_second(void){ uint64_t v; __asm__ volatile("mrs %0,cntfrq_el0":"=r"(v)); return v; }
