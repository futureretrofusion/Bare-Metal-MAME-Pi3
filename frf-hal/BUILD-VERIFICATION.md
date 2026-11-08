# FRF HAL — Studio Main build verification

Copyright © 2026 Future Retro Fusion.

## Source and isolation
- Original v1.15 baseline commit: `34783aeb96a672ff6018563b6c5736080faba7ec`.
- Branch: `frf-independent-hal`.
- Independent Studio Main worktree:
  `/home/fff2025/Documents/Amiga/FRF-BareMetal-MAME-Pi3-FRF-HAL-DEV`.
- Original `main` worktree unchanged. No production kernel/SD writes.
- Compiled with `arm-none-eabi-gcc 14.2.1`.
- ARM32 firmware entry address `0x8000`.
- ARM HYP-to-SVC entry transition implemented for firmware HYP entry.
- Own exception vectors, BSS clearing and mode stacks; 48 MHz-clock
  PL011 UART; BCM2837 1 MHz hardware system timer.
- Stage 1 adds VideoCore property mailbox and double virtual-height
  RGB565 framebuffer (exactly 640x480,
  960-line virtual height).

## Stage 0 (UART/timer bootstrap)
Command: `make -C frf-hal check`

- Clean build and link: **PASS** (exit 0)
- Undefined symbols `arm-none-eabi-nm -u`: **none**
- Circle/CStdlib/libsound/newlib matches in link map: **none**
- Raw image: **1,661 bytes**
- ELF text: **1,661**, data **0**, BSS **21,520** bytes
- Raw image SHA-256:
  `23debf42bc525fb1854acd5592b22c6c28f950fc3bcc7d4ba4eb5c3123cc1672`
- ELF SHA-256:
  `23f0d781505800394a0b481c6ed783a971005073266a84aef771b4640ec1b6e7`

## Stage 1 (mailbox/framebuffer)
Command: `make -C frf-hal check-stage1`

- Clean build and link: **PASS** (exit 0)
- Undefined symbols `arm-none-eabi-nm -u`: **none**
- Circle/CStdlib/libsound/newlib matches in link map: **none**
- Raw image: **3,085 bytes**
- ELF text: **3,085**, data **0**, BSS **21,696** bytes
- Raw image SHA-256:
  `0a8b03692dfe4c8588d91923315dddd7454a31a6457681dcaffdb98ca94adfcb`
- ELF SHA-256:
  `db9bdc7d8fba7aebce61bcef40aac952e80930a86cff18383c07f0ec656584d6`

## NOT yet verified
- No Pi 3 serial/HDMI boot test; **hardware status UNKNOWN**.
- No exception fault-injection test.
- No UART baud, timer drift, 640x480 framebuffer, VBlank or page
  tear/flicker observations on physical Pi hardware.
- `qemu-system-arm` is unavailable on Studio Main.
- No independent SD/FAT, TinyUSB glue, working multicore, HDMI
  audio or full MAME emulator link yet.
- The complete combined binary's third-party licence/trademark
  audit remains open; removing Circle alone is insufficient for
  release clearance.
- The framebuffer mailbox client is a simple, non-reentrant,
  polled bring-up driver. It is not an IRQ-safe shared mailbox service.

## Hardware acceptance
Use a *disposable* Pi 3 test SD with the independently named kernel
and a 3.3 V UART adapter. Preserve boot serial output for each image,
test expected BSS/timer/heartbeat, test a deliberate exception in a
separate build, and photograph/scope the Stage 1 alternating RGB565
test pages. Do not claim either test passed until actual observations
are recorded.

See `README.md`, `STAGE1-TEST.md`, `PLAN.md`.
