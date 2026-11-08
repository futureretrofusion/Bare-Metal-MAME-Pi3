# Circle removal: dependency audit and migration plan

Copyright © 2026 Future Retro Fusion. Recorded against the clean
v1.15 source snapshot (commit `34783aeb96a672ff6018563b6c5736080faba7ec`).

## Architecture finding
`circle-kernel/Makefile` includes Circle-stdlib's `Config.mk`
and Circle's `Rules.mk`; it links Circle libraries, Circle-newlib,
`libsound.a`, MAME archives, zlib and Expat. Merely using TinyUSB for
USB does not remove the Circle runtime. The linked old program depends
on CStdlibAppStdio, CMemorySystem, Circle interrupt/timers,
CBcmFrameBuffer and HDMI sound.

### Replacement inventory

| Existing integration | Source locations | FRF HAL replacement and validation |
|---|---|---|
| ARM entry, mode stacks, runtime and linker | External Circle startup/Rules.mk; `main.cpp` | Firmware-entry ARM32 startup, independent linker, BSS/stack, fault vector; serial output and linker map |
| Physical MMIO, base addresses, time | `baremetal/bm_timer_circle.cpp`, `kernel.cpp` | BCM2837 MMIO + 1 MHz timer; wrap/monotonic tests |
| UART diagnostics | External Circle screen/logger/stdio | PL011 on GPIO14/15; 115200 baud test |
| IRQ and scheduling | `kernel.cpp`; external Circle interrupt | ARM IRQ controller/vector dispatch, safe global IRQ handling and timer IRQ smoke test |
| VideoCore mailbox/framebuffer/page flip | `kernel.cpp`, `screen_dbuf.cpp`, `frf_videocore_edid.inc`, `frf_gui.cpp`, `baremetal/bm_video.c` | BCM2837 mailbox property protocol; allocate 16-bit FB with double virtual height; ordered VBlank/offset flip; test flicker/tearing before UI |
| SD/FAT read/write and POSIX glue | `CStdlibAppStdio`, `mEMMC`, `fatfs/ff.h`, `bm_logger.cpp`, `bm_osd.cpp`, `kernel.cpp` | Independent SDHCI/eMMC driver or vetted permissive driver; correctly licensed FatFs, disk I/O, POSIX `open/stat/readdir` shims; ROM scan + write-back tests |
| TinyUSB USB DWC2 host | `baremetal/tinyusb/frf_tusb_host.cpp` and `third_party/frf_tinyusb_mame` | Retain MIT TinyUSB backend, replace Circle mailbox tags for USB power; verify HID keyboard, pad enumeration and unplug/replug |
| Multicore | `workers.cpp`, `workers.h`, external `CMultiCoreSupport` | Firmware-safe secondary release, per-core stacks, barriers and work queues; CPU1 TinyUSB and CPU2 video smoke test |
| HDMI PCM | `kernel.cpp`, external `CHDMISoundBaseDevice`, `frf_audio_fast.inc` | Independent mailbox/PWM or vetted permissive HDMI PCM implementation; timing, queue and OutRun music regression |
| Runtime allocation + C/C++ support | Circle-stdlib and newlib integration | Explicit C/C++ startup, heap/sbrk, malloc/new, time/file services; audit linked libc licence/exceptions and unresolved symbols |
| Optional VC4 VCHIQ | `frf_vc4_modes.cpp`, VCHI conditional build | Disable initially; replace only if required by functionality |

## Licensing isolation
The existing `circle-kernel/screen_dbuf.cpp` identifies itself as a
GPLv3 Circle-derived file. It must **never** be compiled, copied into,
adapted into or linked with the independent backend without compatible
permission. Preserve it unchanged for the historical Circle build.
Do not move Circle headers, startup, or static libraries into `frf-hal/`.

The MAME 0.106 historical non-commercial terms and notices remain.
Review the precise licensed provenance of FatFs, Expat, newlib, TinyUSB,
legacy MAME subcomponents, SDK firmware interfaces and the use of the
MAME name before any combined binary release.

## Delivery gates
1. **Stage 0 — isolated ARM32 serial boot:** freestanding image builds
   with no Circle symbols; hardware serial boot still needed.
2. **Stage 1 — mailbox + double framebuffer:** firmware framebuffer,
   bounded page-flip/VBlank test, existing FRF GUI renderer adapted.
3. **Stage 2 — IRQ + multicore + TinyUSB:** independent dispatch and
   GPU USB power, then existing USB HID + MAME input compatibility.
4. **Stage 3 — SD/FAT + runtime:** mounting, ROM discovery, safe write
   and C/C++ integration; no ROMs bundled.
5. **Stage 4 — HDMI sound + full MAME link:** preserve original drivers,
   multicore work and video optimisations; hardware/ROM regression tests.
6. **Stage 5 — release review:** verify zero Circle build/link references,
   perform full third-party licence/SBOM review, SD hardware test of
   exact binary and retain source and build reproducibility evidence.

All stages require independent compile/link checks and Pi3 hardware
observations. Never publish a Stage-0 image as a finished arcade kernel.

## Current implementation checkpoint (8 November 2026)
- [x] Independent firmware ARM32 entry (including HYP-to-SVC),
  BSS clearing, per-mode stacks, VBAR vectors, PL011 serial and timer:
  compiled/link verified; **Pi3 boot not yet tested**.
- [x] Independently built firmware mailbox client and double-height
  RGB565 framebuffer test: compile/link verified, HDMI test pending.
- [x] Standalone BCM2837 IRQ controller + compare1 timer test:
  compile/link verified, real interrupt count pending.
- [x] Standalone USB HCD power property test and DWC2 register probe:
  compile/link verified; real Pi3 power test pending.
- [ ] Replace the existing `baremetal/tinyusb/frf_tusb_host.cpp`
  Circle mailbox dependency in a **new** FRF-HAL-target build, without
  editing the working legacy Circle code. Existing FRF HID logic stays.
- [ ] Independent multicore, SD/eMMC and FatFs, HDMI PCM,
  C/C++ runtime + newlib support, full MAME link, functionality tests.
