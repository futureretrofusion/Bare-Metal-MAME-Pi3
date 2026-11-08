# FRF HAL / historical MAME 0.106 — ARM32 hard-float ABI checkpoint

Copyright © 2026 Future Retro Fusion.

**Verification: cross-compilation and ELF inspection only.** No complete
MAME application, Circle-free C/C++ runtime, Pi 3 game boot, HDMI audio
or legal binary release has been verified by this report.

## Motivation

Original MAME core compiled on Studio Main uses:
`-mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard`.

The initial FRF HAL hardware diagnostic binaries used soft-float.
A full build must not blindly mix incompatible ARM calling conventions.

The new `abi-hard.mk` *must be the first makefile passed to make*
because GNU Make expands target paths when reading the rules. It selects
a separate `build-hard/` directory so the original `build/`
soft-float diagnostics are untouched.

## Independently built historical MAME archives

A clean CMake+Ninja out-of-tree build was configured with the unchanged
`cmake/pi3-v115-driver-profile.cmake` and
`toolchain-arm32-circle.cmake` on Studio Main.

```sh
cmake -S mame106 -B build-bm-dkong-arm32-v01 -G Ninja \
  -C cmake/pi3-v115-driver-profile.cmake \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-arm32-circle.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-bm-dkong-arm32-v01 \
  --target mamecore mamecoreUNROLL z expat -j8
```

All four archives now exist and are compiled from the development
worktree source. Earlier 60-second remote command timeouts required
resuming incremental Ninja builds; subsequent build commands returned
exit zero.

| Archive | Size | SHA-256 |
|---|---:|---|
| `libmamecore.a` | 23,931,812 | `59e54626d591fb35e52c9aae1dd4b2caae90ba090bace5373885363f43c09b9a` |
| `libmamecoreUNROLL.a` | 1,456,202 | `780e413be7094b89ae8b2436caa4cb267826e2d751555e342dd2f08d38cf2dd2` |
| `zlib/libz.a` | 115,414 | `de56a11bbe17c91d530b3d019073aa1e6caa0fed87cf4f977974c5ef037c60e9` |
| `libexpat/libexpat.a` | 141,524 | `2775881deb4d7e8a82c9f8be5c10f423deda2d39aa0d80e67eb4d902b710685b` |

`arm-none-eabi-readelf -A` on the MAME `mame.c.obj`
reports `Tag_ABI_VFP_args: VFP registers`.

## Independent HAL matched ABI

```sh
make -C frf-hal -f abi-hard.mk -f Makefile -f gui.mk check-gui
make -C frf-hal -f abi-hard.mk -f Makefile -f gui.mk -f rommenu.mk check-rommenu
make -C frf-hal -f abi-hard.mk -f Makefile check-video check-usbhost
```

All four independent firmware diagnostics compile and link with no
undefined symbols. `arm-none-eabi-readelf -A` reports the exact same
`Tag_ABI_VFP_args: VFP registers` on the HAL GUI and ROM-menu ELF,
and the TinyUSB host ELF.

| Image from `frf-hal/build-hard/` | SHA-256 of raw binary |
|---|---|
| `frf-hal-gui.img` | `0b9533793b8e21862b1fe036fc6cd6a64c0dcac4b4fe46e6cbef88caec11a74a` |
| `frf-hal-rommenu.img` | `3afee4f0693b60c7c5097c5daa03dbf4dcff14120ac8c55f280859fe9fd120e7` |
| `frf-hal-video.img` | `d9bbda585aae0d75c32b06df6281dce329817fe974bc80c1fe74f90acfcf5604` |
| `frf-hal-usbhost.img` | `af203efd35cd21d435982d5c4d2a0e3f22392dcaf0ccfffad681d0cf1f7d1846` |

The hard-float profile defines `FRF_HAL_HARD_FLOAT=1`, which
conditionally enables CP10/CP11 access through CPACR and FPEXC.EN
on CPU0 and on each secondary CPU after SVC transition. Disassembly
of the hard-float startup confirms these instructions are emitted.
The original soft-float Stage-0 build also rebuilt successfully
without this macro.

## What remains unresolved

* Pi 3 firmware's initial CPACR/NSACR/HCPTR state, ARM HYP traps,
  board-specific boot paths and actual VFP execution require
  hardware validation before any claim of runtime correctness.
* Matching toolchain ABI is a prerequisite, **not** proof that the full
  MAME executable links. Its runtime/file/alloc/libm/C++ ABI,
  existing OSD frontend, SD/ROM loader, HDMI PCM, TinyUSB input
  and multicore scheduler still need complete integration.
* Circle/Circle-stdlib may not be included in the eventual
  standalone executable. The legacy MAME licence remains historical
  noncommercial and upstream copyrights must be retained.
* No ROMs, BIOS, CHDs or game artwork were added to this branch.
