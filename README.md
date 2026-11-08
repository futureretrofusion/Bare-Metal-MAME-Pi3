# Future Retro Fusion — Bare Metal MAME for Raspberry Pi 3
**Source snapshot: development v1.15 (8 November 2026)**

Standalone bare-metal arcade emulation development for Raspberry Pi 3 hardware, derived from MAME 0.106. It includes project-specific Circle ARM32 frontend, USB integration, multicore support work, arcade menu and video pipeline changes.

**Status:** Development source snapshot, not a verified public binary release. v1.15 has compiled locally but has not yet been hardware-tested; game compatibility and performance vary.

## Tree
- `mame106/` — historical MAME 0.106 derivative with local game/driver and build changes.
- `baremetal/` — bare-metal framebuffer/input/timer implementation and TinyUSB host integration.
- `circle-kernel/` — Circle-based Pi 3 application and build Makefile.
- `third_party/` — vendored TinyUSB, zlib, and the source of the shared Expat build component.
- `toolchain-arm32-circle.cmake` — cross-compile configuration.
- `LICENSES/` — principal component licence copies.

## Build context
The kernel Makefile presently relies on a separately installed, specifically configured Circle-stdlib and its Circle dependency. The development setup used the upstream Circle-stdlib revision `4fbab627a86f2bfba1697c1b4d3f310c48be27aa` and Circle revision `7a81e1b480fe7d19fc1fc695b89bff0a0848f44b`. The Makefile defaults to `../circle-stdlib`. Obtain and configure Circle-stdlib outside this repository at the pinned source revision, or pass `CIRCLESTDLIB=/path/to/circle-stdlib` to `make`.

The MAME core was configured through CMake with the provided ARM32 toolchain and linked via `circle-kernel/Makefile`. Actual toolchain paths and build flags need verification on a clean machine. This package is an archival source snapshot. The previously external Expat path has been made local to the release tree; the external Circle-stdlib build dependency remains.

## Copyright and licensing
Copyright © 2026 Future Retro Fusion for its original project-specific contributions only. Original MAME 0.106, Circle, TinyUSB, zlib, expat and any other upstream rights remain with their original holders. Licence terms of the respective files and components govern their reuse. See `LICENSES/` and source-file notices.

**Important:** The MAME 0.106 historical licence disallows commercial use; this snapshot also references GPLv3 Circle/Circle-stdlib components. Licensing compatibility of the linked resulting program is **not yet cleared**. Do not distribute a combined binary or claim the entire tree is proprietary/GPL without resolving this.

No commercial game ROM sets, BIOS, CHDs or bundled arcade game artwork are included. Users must supply legally obtained game data and respect applicable rights.

## Provenance
Assembled directly from the Studio Main development tree `Mame106RGB-BareMetal-Pi3-v0.1`, updated in place through v1.15. The associated v1.15 directory is an incremental release snapshot; its additional source edits already match the main development source and have been checked by the release script. Development history and original copyright headers have not been rewritten.

## Development build checklist
1. Install `arm-none-eabi-gcc`, `arm-none-eabi-g++`, CMake, GNU Make and the matching Circle libraries.
2. Obtain and configure the specified Circle-stdlib and Circle revisions separately (see the original upstream projects).
3. Compile the MAME core from the project root:
   ```sh
   cmake -S mame106 -B build-bm-dkong-arm32-v01 -C cmake/pi3-v115-driver-profile.cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-arm32-circle.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build-bm-dkong-arm32-v01 --target mamecore mamecoreUNROLL z expat -j4
   ```
4. The Circle kernel Makefile expects the MAME core libraries in `build-bm-dkong-arm32-v01`; the commands above use that layout. The driver profile is required: the original CMake defaults turn on many legacy drivers that fail with the ARM compiler.
5. Build the Circle application with `make -C circle-kernel CIRCLESTDLIB=/path/to/circle-stdlib`.
6. Test the generated kernel on a Raspberry Pi 3 with authorised ROM data. No ROM images are included.

Clean ARM32 compilation, including the two MAME archives, zlib, Expat, and Circle kernel link, passed on Studio Main on 8 November 2026. Hardware boot of this reconstructed build remains untested. The combined binary's third-party licence compatibility needs resolution before binary distribution.
