# FRF HAL independent Pi3 hardware acceptance (PENDING)

Copyright © 2026 Future Retro Fusion. All items unchecked until
actual serial log/video observation on a *disposable* Raspberry Pi 3 SD.

## Safety and boot configuration
- Keep production v1.15 card and Circle binary untouched.
- Copy only the selected test image from `frf-hal/build/` to a
  separate, backed-up FAT firmware boot partition.
- On that test SD only use:

```ini
arm_64bit=0
kernel=frf-hal-stage0.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

Change `kernel=` to **one** selected file below.
GPIO14 TX header pin 8 -> 3.3V TTL adapter RX; pin 6 -> ground.
115200 8N1. Do not connect adapter 5V or RS-232 signals.
Log power-on to steady heartbeat; photograph HDMI output when relevant.

| Test image | Expected signal | Acceptance |
|---|---|---|
| `frf-hal-stage0.img` | UART banner, BSS, timer heartbeat | stable serial and ~1s interval |
| `frf-hal-stage1.img` | double-height framebuffer red/blue alternating | both pages seen, no corruption |
| `frf-hal-stage2.img` | timer1 IRQ count | ~100 interrupts/second, 0 unexpected |
| `frf-hal-usbpower.img` | GPU power ACK, DWC2 SNPSID/HPRT | firmware ACK + plausible ID |
| `frf-hal-mmu.img` | MMU/cache RAM probe + UART heartbeat | no exception, steady timing |
| `frf-hal-smp.img` | core1–3 ACK and IPC | three START/IPC PASS, no timeout |
| `frf-hal-irq-router.img` | shared dispatch IRQ count | timer ~100/s, no unhandled IRQ |
| `frf-hal-usbhost.img` | TinyUSB hub, keyboard/HID attach | mount, reports/mask, unplug/replug |
| `frf-hal-video.img` | original FRF renderer: animated 2x image, dark scanlines | scanline/geometry correct, sustained flips; visually inspect tearing |

Notes: **These expected signals are not observed results**. The MMU
diagnostic enables D-cache; do not combine it with USB or GPU DMA tests.
The USB and video images run uncached. Controller IDs needing nonstandard
proprietary handshakes may not work yet. No full MAME emulator is linked.
HDMI VBlank/page synchronization needs sustained 50Hz display validation.

## Remaining integration blocks
- [ ] SD/eMMC block IO, FatFs mount, directory iteration and ROM access
- [ ] C/C++ allocator and POSIX/newlib integration without Circle
- [ ] Unified multicore scheduler, USB IRQ and DMA coherency
- [ ] Full HID keyboard/pad reports mapped to original FRF menu/MAME
- [ ] HDMI PCM/VCHIQ-independent audio backend and original music
- [ ] Full MAME 0.106 link, boot/ROM/game compatibility and licensing audit
