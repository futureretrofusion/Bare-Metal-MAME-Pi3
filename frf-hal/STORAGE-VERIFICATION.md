# FRF Independent HAL — Storage verification

Copyright © 2026 Future Retro Fusion. SPDX-License-Identifier: MIT.
Project branch: `frf-independent-hal`. Verification on Studio Main,
8 November 2026.

## Compiled output

- `make -C frf-hal -B check-storage`: PASS.
- `frf-hal/build/frf-hal-storage.elf`: ARM32 Cortex-A53 freestanding.
- ELF text: **12,084 bytes**; data: **4 bytes**; BSS: **69,344 bytes**.
- `arm-none-eabi-nm -u`: no undefined symbols.
- Circle/Circle-stdlib/screen_dbuf/newlib/libsound literal name matches
  in the storage link map: none.
- Raw image SHA-256:
  `175495e7ea718841a402e088db7ec08c1f82cd5735d7d226e9af906a81c544a6`.
- ELF SHA-256:
  `ea2dde2e70cd0b97b45901e2a266e9c7beb1ffe9f1322d99497fa6fe251e6499`.

## Host test results

`make -C frf-hal -B ...` succeeded with:

| Test | Result |
|---|---|
| `host-test-fat32`: synthetic MBR, FAT cluster chain, VFAT and errors | PASS |
| `host-test-sdhost`: SD command flags, SDSC/SDHC address conversion | PASS |
| `host-test-catalog`: ZIP scanning, BIOS exclusion, duplicate filter and menu title sort | PASS |
| `host-test-fat32-image`: actual formatted 80 MiB FAT32 test image, MBR-wrapped | PASS |
| `host-test-file-pool`: file open, multi-handle read, seek, EOF and exhaustion on formatted FAT32 MBR | PASS |

The FAT32, ROM catalog and file pool host tests were compiled with
`-fsanitize=address,undefined` and all completed successfully.

Also verified separately with a formatted FAT32 superfloppy scratch
image. Source ZIP fixture files were FRF-written text strings, not game ROMs.
All disk images were temporary regular files in `frf-hal/build/`,
not `/dev` or the production MAME card.

## Correct limitations

The SDHOST driver compiled and linked; it has **not been tested on
Raspberry Pi 3 physical hardware**. This is not proof of SD card
initialization, PIO FIFO correctness, physical sector reading,
peripheral GPIO mux, FAT32 mount on the card, or loaded games.

The ROM catalog presently uses a seven-driver demonstration table
in the storage diagnostic, not the complete MAME 0.106 driver list.
Full MAME OSD bindings, ROM ZIP content handling, FAT writes and
HDMI audio remain unimplemented.

See `STORAGE-TEST.md` for reproducible UART hardware test instructions.

## Full SDHOST MMIO state-machine simulation

Command:

```sh
make -C frf-hal host-test-sdhost-mock
```

Result: **PASS** (native GCC with AddressSanitizer + UBSan).
The test uses the actual `sdhost.c` command/init/read logic with
a fake memory-mapped SDHOST controller; verifies both SDHC
and SDSC command sequences, a 512-byte PIO read, data error
propagation, invalid RCA and bounded timeout handling.
No physical Pi3 registers or SD card were accessed.

The test target is guarded by `FRF_SDHOST_MOCK` and is not
compiled into the ARM32 firmware. The original storage
image's SHA-256 is unchanged after the mock backend was added.
