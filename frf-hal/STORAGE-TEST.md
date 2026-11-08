# Future Retro Fusion — Pi 3 independent storage diagnostic

Copyright © 2026 Future Retro Fusion. SPDX-License-Identifier: MIT.

**Status 8 November 2026:** ARM32 binary linked without Circle;
host FAT32/ROM-index/file-handle tests passed. **SD hardware initialization,
PIO sector reads, and mounting a real Pi 3 card are NOT hardware-tested.**

## What has been implemented

- An independent BCM2837 **SDHOST** driver at ARM physical
  `0x3F202000` (not Arasan/SDHCI at `0x3F300000`).
  It requests 1-bit SD mode, initializes with bounded command timeouts,
  and implements only the SD card read command CMD17.
- CMD0, CMD8, CMD55/ACMD41, CMD2, CMD3, CMD7 and CMD16 (SDSC only)
  startup sequence. SDHC/SDXC and byte-addressed SDSC arguments supported.
- A new **read-only, independently implemented FAT32 reader** supporting
  MBR FAT32 primary partitions (0x0B/0x0C) and superfloppy layouts,
  512-byte sectors, cluster chains, directory traversal, 8.3 and ASCII
  VFAT long filenames, and random-access reads.
- An independent ROM directory index adapter scans `/roms/*.zip`,
  excludes the `neogeo` shared BIOS entry, deduplicates matches and
  sorts by game title. It takes a game-driver table as an argument.
- A read-only 32-slot file-handle API supports `SD:/roms/galaga.zip`,
  interleaved reads, file size, tell/seek and EOF, with no malloc.
- The SD diagnostic uses a **seven-driver pilot set**, not the full
  MAME 0.106 driver list, and does not attempt to execute any ROM.

## Build and link verification

From the separate `frf-independent-hal` worktree:

```sh
make -C frf-hal check-storage
make -C frf-hal host-test-fat32 host-test-sdhost host-test-catalog
```

`frf-hal/build/frf-hal-storage.img` is the ARM32 output. The ELF has
no undefined symbols. Its link map must contain no Circle,
Circle-stdlib, libc or proprietary emulator driver objects.

The host tests are synthetic for FAT32 MBR, LFN, directory traversal,
seek, corrupted FAT chains and I/O faults. In addition, the filesystem,
ROM index and file pool were tested against a **freshly formatted
80 MiB scratch FAT32 image** created with `mkfs.fat` and populated
with `mtools`. Both superfloppy and MBR-wrapped scratch images passed.

Never run `mkfs.fat` or the fixture tools against `/dev/sdX`;
these tests only write to new `frf-hal/build/` image files.

The actual tests can be rerun with:

```sh
make -C frf-hal host-test-fat32-image \
  FRF_FAT32_IMAGE=build/frf-fat32-test.img
make -C frf-hal host-test-file-pool \
  FRF_FAT32_IMAGE=build/frf-fat32-mbr.img
```

The scratch fixtures are **not** stored in Git and are not distributable
arcade ROM archives. They contain small FRF-written test strings.

## Physical Pi 3 bring-up (strictly disposable test SD)

1. Do **not** alter the production v1.15 MAME SD card, its kernel, or
   the working Circle tree.
2. Prepare a separate, backed-up test card with compatible Pi 3 boot
   firmware and a FAT32 partition. The firmware boot partition may be
   FAT16, but this diagnostic's parser currently scans FAT32 only.
3. Copy `frf-hal-storage.img` to that test card's FAT boot partition.
4. On that **test card only**, select it in `config.txt`:

```ini
arm_64bit=0
kernel=frf-hal-storage.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

5. Capture UART at 115200 8N1 from GPIO14 (pin 8) and GND (pin 6),
   using a **3.3V TTL** adapter, not RS-232 voltages.
6. Record the startup CMD/STATUS/RESP diagnostics, RCA/OCR,
   LBA0 signature, FAT32 BPB and root `/roms` entries. A compile-only
   success is *not* card-detection or file-read success.

The driver uses the Pi 3 SDHOST pin group GPIO48–53; firmware board
configuration and pin mux must match. The Arasan SDHCI controller may
be used for Wi-Fi, depending on model and firmware. GPIO ownership and
DWC2/VideoCore interactions still require board validation.

## Limitations, blockers and licensing

- **No verified physical SD card read**. PIO FIFO timing, SDCLK divider,
  GPIO mux, command response handling and SDHC/SDSC negotiation require
  Pi 3 UART output and correction if needed.
- Read-only only: MAME settings, logs, index caches and high scores
  cannot be written through this interface.
- FAT32 only, ASCII VFAT only. FAT12, FAT16, exFAT, UTF-16 non-ASCII
  names, GPT partitions and partition logical-volume chains are absent.
- Single-core, non-reentrant filesystem cache. No MMU/D-cache, IRQ
  handler, DMA, HDMI audio or full MAME core integrated into this image.
- ZIP **archive presence** is not ROM CRC verification. MAME ZIP file
  handling, full driver table, GUI selection and game launch remain open.
- **No Circle, Circle-stdlib or GPL SDHOST implementation copied.**
  The numeric BCM2837 register layout and SD command identifiers
  are protocol facts; the implementation is original FRF code.
  Source provenance still requires a full review before distribution.
- The historical MAME 0.106 non-commercial license remains unchanged.
  No copyrighted game ROMs may be bundled in a binary or repository.

Public hardware references:
- Raspberry Pi BCM2835 ARM Peripherals register descriptions:
  https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf
- Raspberry Pi Pi2/Pi3 SDHOST versus Arasan physical base discussion:
  https://raspberrypi.stackexchange.com/questions/50793/
  physical-address-address-of-sd-host-controller/50798

**Next:** verify Pi 3 SDHOST reads from a disposable card, then
provide a unified ROM/GUI kernel and full MAME 0.106 OSD file adapter.
