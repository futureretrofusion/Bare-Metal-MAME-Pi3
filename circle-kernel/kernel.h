#ifndef MAME106RGB_KERNEL_H
#define MAME106RGB_KERNEL_H

#include <circle_stdlib_app.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbgamepad.h>
#include "workers.h"

class CKernel : public CStdlibAppStdio
{
public:
    CKernel(void);
    bool Initialize(void) override;
    TShutdownMode Run(void) override;

public:
    static void KeyStatusHandlerRaw(unsigned char modifiers, const unsigned char rawKeys[6]);
    void ServiceUSBPlugAndPlay(void);
private:
    static void KeyboardRemovedHandler(CDevice *device, void *context);
    static void GamePadStatusHandler(unsigned deviceIndex, const TGamePadState *state);
    static void GamePadRemovedHandler(CDevice *device, void *context);
    void AttachUSBInput(void);

    CMameWorkers mWorkers;
    CUSBKeyboardDevice * volatile mKeyboard;
    CUSBGamePadDevice * volatile mGamePad;
    static CKernel *sThis;
};

#endif
