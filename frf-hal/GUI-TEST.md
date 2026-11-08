# FRF HAL — original graphical menu integration test

Copyright © 2026 Future Retro Fusion. The original `frf_gui.cpp`,
font and banner remain in place and unchanged in `circle-kernel/`.
Those FRF-authored assets do not depend on Circle's APIs.

The test builds `frf-hal-gui.img` by compiling the existing
`circle-kernel/frf_gui.cpp` directly against the independent
1280×720 RGB565 framebuffer path. `mini_runtime.c` supplies only
the small functions used by the demo GUI; it is **not** a complete
libc/newlib replacement.

```sh
make -C frf-hal check-gui
```

Test on a **disposable** Pi 3 boot SD only, not the v1.15 card:

```ini
arm_64bit=0
kernel=frf-hal-gui.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

A successful test would show the original FRF menu at 1280×720
with FRF branding, a 14-line diagnostic-only selection, and a moving
selection highlight every 300 ms. The 720p double-buffer scanout uses
the independent mailbox code. This diagnostic does **not** enumerate
ROMs, use real USB inputs, run games or provide sound.

No Pi 3 hardware observations have been made; compile/link success
must not be represented as a working graphical frontend until the
exact test binary boots and HDMI output is inspected.

This is **not** a replacement of the Circle production frontend.
All old files remain in place, and the complete MAME binary is not
licensed/verified for distribution.
