# FRF HAL independent TinyUSB host — Pi 3 test protocol

Copyright © 2026 Future Retro Fusion. This diagnostic is independently
integrated using the existing MIT-licensed TinyUSB source tree.

## Build and actual status

From the independent `frf-independent-hal` worktree:

```sh
make -C frf-hal check-usbhost
```

Output: `frf-hal/build/frf-hal-usbhost.img`. This diagnostic is an ARM32
kernel, **not** a playable MAME binary and **not** approved for release.
The 8 November 2026 cross-compile and link passed, including the MIT
TinyUSB host, DWC2, hub and HID code. There is no physical Pi 3 boot,
hub enumeration, keyboard or gamepad evidence yet.

## Test preparation — separate disposable SD only

Never modify the production MAME SD card, working v1.15 kernel, or
the `main` branch. The disposable card must contain compatible Pi 3
firmware boot files. Copy the `frf-hal-usbhost.img` image to that
separate FAT boot partition, and use on that card only:

```ini
arm_64bit=0
kernel=frf-hal-usbhost.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

Connect a 3.3 V TTL adapter: Pi GPIO14/header pin 8 → adapter RX,
Pi header pin 6 → adapter ground; use 115200 8N1. **Never connect
RS-232 voltage or the adapter's 5 V output**.

## Expected serial observations (NOT hardware results)

- `Circle-free TinyUSB host diagnostic` banner
- DWC2 power firmware acknowledgment and a plausible Synopsys ID
- `TinyUSB host initialized` if host initialization returns success
- Periodic diagnostic counters: live devices, mounts, unmounts,
  HID interface mounts, HID reports, enumeration errors
- Plug/unplug a USB keyboard and supported HID pad: compare counters,
  nonzero report sizes and first four raw report bytes

A hub may appear before the keyboard or pad because the Pi 3 USB
ports share an onboard LAN9514 hub. Capture full UART output from
power-on, including cases where TinyUSB init fails. An initialized
host is **not proof of USB enumeration**.

## Technical boundaries

- Runs entirely on primary CPU, using polled DWC2 interrupt handling.
  The ARM interrupt router and multicore USB ownership are not integrated.
- Firmware mailbox and DWC2 DMA mapping assume MMU and data caches
  disabled during this diagnostic. Cache hooks are barriers only.
  **Do not reuse in a cached memory configuration.**
- HID events and first report bytes are observable, but FRF's existing
  keyboard/gamepad mapping and MAME input are not linked.
- Original legacy `circle-kernel` is not modified or linked.
- Real hardware and exact image checksums must be recorded before
  a test may be labeled PASS.
