> **Latest verification supersedes old size/hash entries below.**
> Expanding `boot.S` for secondary cores changed the diagnostic binary
> layout; previously recorded historical checksums remain archival.
> The authoritative current image hashes appear at the end of this file.

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

## Independent TinyUSB host diagnostic (8 November 2026)
Command: `make -C frf-hal check-usbhost`.

- ARM32 compile/link: **PASS**, GNU Arm Embedded 14.2.1.
- Freestanding image: `frf-hal/build/frf-hal-usbhost.img`.
- Executable ELF text **29,623**, data **57**, BSS **27,360** bytes.
- `arm-none-eabi-nm -u`: **no undefined symbols**.
- Circle/Circle-stdlib/libsound/newlib name matches in link map: **none**.
- Linked vendor pieces: MIT TinyUSB core, FIFO, USB host, hub, HID,
  Synopsys DWC2 HCD and DWC2 common; independent boot/USB power/runtime.
- Image SHA-256:
  `a695b02f8d27a580f84309ac2108b18ceeec228cbe321e28890edb939ddb3fa9`.
- ELF SHA-256:
  `94d0a91768d697308542b39eb43db431dec4a76732b232bc8e2dabb2281803ef`.
- **Hardware boot, DWC2 initialization, hub and HID enumeration:
  NOT TESTED.** No MAME runtime or gamepad input is integrated.
- The existing Stage 0, Stage 1, Stage 2 and USB-power check targets
  were re-run successfully after adding this independent target.
- See `USBHOST-TEST.md` for safe test-card procedure.

## Current clean-build matrix — 8 November 2026

Command executed on Studio Main:

```sh
make -C frf-hal clean
make -C frf-hal check check-stage1 check-stage2 check-usbpower \
  check-usbhost check-mmu check-smp check-video check-irq-router \
  host-test-mmu host-test-smp host-test-input host-test-dma \
  host-test-irq-router
```

**Result: PASS (exit 0)**, 9 ARM32 firmware images linked and 5 native
host protocol/logic tests ran to completion. The independent UART,
mailbox, TinyUSB/DWC2, FRF HID adapter, DMA, original FRF scaler and
IRQ router contain no external Circle link dependencies in these images.

| Image | ARM ELF text | BSS | SHA256 of raw image |
|---|---:|---:|---|
| stage0 | 1849 | 33808 | `06efc4afa348d492414f9df505113b31ec1f926300da9390e7e6d0ee4fc4d656` |
| stage1 | 3273 | 33984 | `21c259fe56ac55be4989522788311e255c79135cecb85be4bcaadfaa3260ed4b` |
| stage2 | 2017 | 33808 | `01a2ae434c87768bebb6de191f8d2d9894863dc666597f6734a09eccfe0457a2` |
| usbpower | 2441 | 33824 | `f0d57303eca19ca6f98b6e4fc2f7bb66ad9da1d55c0316d8d4da62d63fd86a0e` |
| usbhost | 35159 | 40160 | `c00b79d386e815755efe580bad74856207a2d5c68a8e394cb6572b077d3064eb` |
| mmu | 2093 | 67584 | `54c7d6fe91a8830ab4b189e88c7b4edbaef86ee2c5b9d30c849a1d2c96261720` |
| smp | 2605 | 33872 | `dc054c95d54478bc543c10ea2fd55e770e0ad48893d2bad4b2c9557aa883586e` |
| video | 7125 | 581088 | `e6394cca6fc9c13f2cbd4b29b1366241765b3e0bfadbd8c43d63983197d93552` |
| irq-router | 2377 | 34384 | `e8e8ea5f787589fbb0cffbb45ddf9aab5b683bd65de7796160338b16f1c42baf` |

- All nine `arm-none-eabi-nm -u` checks reported no unresolved symbols.
- Regex audit of five expanded link maps for
  Circle/Circle-stdlib/libsound/newlib/screen_dbuf names returned zero matches.
- Original `baremetal/bm_video.c` emits **one unused-variable warning**
  under strict GCC 14.2.1; local `-Wno-error=unused-variable` allows
  compiling it **without altering legacy source**.
- **Hardware boot: NOT TESTED for any of these binaries.**
- **No emulator CPU core, ROM files, SD/FAT or HDMI PCM in these images.**
- DMAC/cache helpers must not be treated as validated coherent DMA on Pi 3;
  D-cache remains disabled in TinyUSB and framebuffer images.
- Independent HAL binary release remains **not legally cleared**; full
  provenance, licence, name/wordmark and exact linking review is required.

- The startup release flag is now initialized in the loaded `.data`
  image, not in uninitialized BSS. This prevents secondary cores from
  misreading stale memory before CPU0 has cleared the BSS. All nine
  ARM32 targets and all five host tests passed again after that change.
