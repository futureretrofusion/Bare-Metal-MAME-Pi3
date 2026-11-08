#ifndef MAME106RGB_WORKERS_H
#define MAME106RGB_WORKERS_H

#include <circle/multicore.h>
#include <circle/memory.h>

class CMameWorkers : public CMultiCoreSupport
{
public:
    CMameWorkers(CMemorySystem *pMemorySystem);
    void Run(unsigned nCore) override;
};

#endif
