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
- Raw image: **1,689 bytes**
- ELF text: **1,689**, data **0**, BSS **21,520** bytes
- Raw image SHA-256:
  `5c338cc4a9ebc923be600de041e2727de7048a3a32e29d8405dd8573ae3d635f`
- ELF SHA-256:
  `43156514f9f5dc7a791d7ccff99628efb4c1f50832d791156502ef0139724159`

## Stage 1 (mailbox/framebuffer)
Command: `make -C frf-hal check-stage1`

- Clean build and link: **PASS** (exit 0)
- Undefined symbols `arm-none-eabi-nm -u`: **none**
- Circle/CStdlib/libsound/newlib matches in link map: **none**
- Raw image: **3,113 bytes**
- ELF text: **3,113**, data **0**, BSS **21,696** bytes
- Raw image SHA-256:
  `065c891345d733c019d4b21f700e695ba39018be108061c5a2e75fe13ffc0f73`
- ELF SHA-256:
  `e51a5a096a95c892f5aaa1fd763a8e8d1091b59436ee0e99e492c0afa906bb5a`

## Stage 2 (interrupt vector + system timer compare channel 1)
Command: `make -C frf-hal check-stage2`

- Clean build and link: **PASS** (exit 0)
- Undefined symbols `arm-none-eabi-nm -u`: **none**
- Circle/CStdlib/libsound/newlib matches in link map: **none**
- ARM IRQ entry saves/restores volatile registers; a dedicated C handler
  acknowledges timer compare1 and re-arms it approximately every 10 ms.
- Raw image: **1,861 bytes** (text 1,857 + initialised data 4).
- ELF text **1,857**, data **4**, BSS **21,520** bytes.
- Raw image SHA-256:
  `a607157db48882967b2cc2f2fb3b3b559e6fb7c8c0e5ca394ea27f2c88ee71b2`
- ELF SHA-256:
  `c359be0e62d4186b11b26b1dbc67201f866640a300056c02e444ad1f298862f8`
- The IRQ diagnostic is not suitable as a shared IRQ controller: it
  disables other interrupt banks and treats unexpected sources as errors.
- Physical Pi 3 IRQ frequency and interrupt behaviour: **NOT TESTED**.

## USB-power diagnostic (separate image)
Command: `make -C frf-hal check-usbpower`

- Clean ARM32 compile/link: **PASS** (exit 0)
- Undefined symbols `arm-none-eabi-nm -u`: **none**
- Circle/CStdlib/libsound/newlib matches in link map: **none**
- Raw image: **2,281 bytes**; BSS **21,536** bytes
- Raspberry Pi firmware power tag `0x00028001`, USB HCD device 3,
  on+wait; diagnostic reads BCM2837 DWC2 core ID and port register.
- Raw image SHA-256:
  `d8e876d06bf122bd5ea585abc75b758107b9dd852b26fa3c65ddc35815bb0e53`
- ELF SHA-256:
  `02b2995c3cceb59bbec94ca512292c5dbbc7d6b130d92c7b74885f225bbff4d2`
- This is **not** a compiled TinyUSB host; no pads/USB devices are
  enumerated in this diagnostic.
- Physical Pi 3 USB power property + DWC2 ID: **NOT TESTED**.

## NOT yet verified
- No Pi 3 serial/HDMI boot test; **hardware status UNKNOWN**.
- No exception fault-injection test.
- No UART baud, timer drift, 640x480 framebuffer, VBlank or page
  tear/flicker observations on physical Pi hardware.
- `qemu-system-arm` is unavailable on Studio Main.
- No independent SD/FAT, integrated TinyUSB host, multicore, HDMI
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

See `README.md`, `STAGE1-TEST.md`, `STAGE2-TEST.md`, `USBPOWER-TEST.md`, `PLAN.md`.
