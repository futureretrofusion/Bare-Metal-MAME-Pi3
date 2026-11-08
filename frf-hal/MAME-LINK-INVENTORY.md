# FRF HAL — MAME 0.106 archive external-symbol inventory

Copyright © 2026 Future Retro Fusion.

**Status: read-only archive analysis; not a successful full MAME link.**
Run the independently authored script from the source worktree:

```sh
python3 frf-hal/tools/mame_archive_audit.py build-bm-dkong-arm32-v01
```

The script runs `arm-none-eabi-nm -g --defined-only` and
`--undefined-only` on the freshly rebuilt historical MAME 0.106
archives and their zlib/Expat libraries. It subtracts all symbols
defined across those four archives from their union of references.
This is source-matched static inventory, **not** proof of the final
selected-object link or a legal determination.

## Observed archive symbol inventory — 8 November 2026

| Library | Distinct globally defined | Referenced undefined |
|---|---:|---:|
| `libmamecore.a` | 15,146 | 11,528 |
| `libmamecoreUNROLL.a` | 1,218 | 299 |
| `libz.a` | 104 | 46 |
| `libexpat.a` | 80 | 19 |

Union: **16,548 defined**, **11,738 referenced**, and
**142** unique names referenced by these archives but not defined
by any of them.

| External family | Unique names | Replacement/integration plan |
|---|---:|---|
| FRF/MAME OSD and hooks | 42 | Reuse FRF original OSD adapter only after separating POSIX/stdio, ROM file, sound and existing video functions from Circle |
| C runtime / file services | 36 | Native ARM EABI-compatible newlib and independent FAT32-backed file, allocation and POSIX service layer |
| C++ / ARM EABI | 19 | Audit libstdc++ and C++ ABI needs and exception/rtti expectations; do not assume -fno-exceptions eliminates every stdlib symbol |
| Maths | 15 | Target-matching `libm`, evaluate performance and floating-point startup |
| Other implementation calls | 30 | Confirm `time`, `setjmp`, `clock`, FRF counters, display globals and formatting, including library provenance |

The four libraries themselves internally resolve thousands of
references, but archive members are selected on demand, so the
missing-symbol inventory over-approximates the functions a particular
driver profile or executable entry will actually require.

Important recorded OSD hooks: `osd_init`,
`osd_get_path_info`, `osd_fopen`, `osd_fread`,
`osd_fseek`, `osd_update_video_and_audio`,
`osd_get_code_value`, `osd_cycles`,
`osd_start_audio_stream`, `bm_cycles`,
`bm_debug_progress`, `frf_skip_frame_begin`.

The original FRF `baremetal/bm_osd.cpp` already provides much
of this API but uses `FILE*`, `stat`, `malloc`, `printf` and
other runtime services previously supplied by Circle-stdlib/newlib.
A correct independent backend must implement compatible file,
allocation, timer and sound services rather than just use dummy
stubs for ROM data.

## Prerequisites for actual full link and hardware test

1. Complete an independent ARM32 hard-float runtime/startup with
   VFP/NEON enabled on each core, heap and C++ construction support.
2. Provide legitimate libc, libm, and C++ runtime components with
   their precise licences and corresponding source/exception terms.
3. Add independently implemented FAT32-backed ROM file operations
   matching MAME's `osd_fopen/fread/fseek` and directory APIs.
   ZIP validity and ROM CRC auditing must happen inside MAME.
4. Connect preserved FRF software video, scanlines, 720p menu,
   buffered output, TinyUSB gamepads and multicore scheduling.
5. Implement hardware-tested HDMI PCM; test YM2151/OutRun audio
   regressions rather than asserting sound works without a device.
6. Link the exact MAME archives without Circle objects/libraries,
   inspect the real linker map for unresolved symbols/GPL components,
   run test ROMs on disposable Pi3 media and record binary hashes.
7. Preserve historical MAME 0.106 noncommercial licence/upstream
   copyrights and complete a distribution/trademark review.

None of the above requires overwriting the working v1.15 kernel or
production SD card. Existing Pi 3 hardware tests remain incomplete.
