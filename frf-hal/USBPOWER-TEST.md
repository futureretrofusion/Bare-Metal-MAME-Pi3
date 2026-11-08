# FRF HAL — independent Pi 3 USB power diagnostic

Copyright © 2026 Future Retro Fusion (MIT original HAL code).

This **test image** does not link TinyUSB, MAME, Circle or Circle-stdlib.
It exercises only the firmware USB HCD power-control property, then reads
the BCM2837 Synopsys DWC2 core identifier and host port register.

Build: `make -C frf-hal check-usbpower`.

Use **disposable test media**, not the original v1.15 SD:
```ini
arm_64bit=0
kernel=frf-hal-usbpower.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

At 115200 8N1 over **3.3 V TTL** UART, a successful result should show:
- `USB HCD power ACK: YES`
- DWC2 SNPSID with high 16 bits `0x4F54`
- Host port register and repeating heartbeat.

**Do not claim this is a working TinyUSB/gamepad driver.** The current
working TinyUSB HID host integration is kept unchanged in
`baremetal/tinyusb/frf_tusb_host.cpp`; this diagnostic simply
replaces the Circle-specific *USB power request* with an FRF-owned
permissively licensed component. Runtime integration must still provide
DMA-safe buffers, memory/cache services, USB host scheduling, callback
bridges, and multicore launch.

The implementation uses the public Raspberry Pi firmware mailbox
property `Set power state`, tag `0x00028001`, device ID `3`,
request state `3` (ON + WAIT), then checks the response.

Reference: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

**Hardware state remains untested** until a genuine Pi3 UART capture.
