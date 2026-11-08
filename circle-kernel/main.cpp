#include "kernel.h"
#include <circle/startup.h>

int main(void)
{
    CKernel Kernel;
    if (!Kernel.Initialize())
    {
        halt();
        return EXIT_HALT;
    }

    CStdlibApp::TShutdownMode mode = Kernel.Run();
    if (mode == CStdlibApp::ShutdownReboot)
    {
        reboot();
        return EXIT_REBOOT;
    }

    halt();
    return EXIT_HALT;
}
