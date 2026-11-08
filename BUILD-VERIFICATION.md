# Studio Main build verification — 8 November 2026

- Main source snapshots against v1.15 incremental tree: `frf_gui.cpp`, `mamedriv.c`, `gamedrivers.cmake` matched SHA-256 byte-for-byte.
- Source export excludes prepared-ROM ZIPs, CHDs, binaries and back-up archives.
- CMake clean configure passed with `cmake/pi3-v115-driver-profile.cmake` (32 enabled driver groups).
- `mamecore` and `mamecoreUNROLL` targets both built successfully with arm-none-eabi GCC 14.2.1.
- `z` (zlib) and `expat` static library targets both built.
- Circle 32-bit kernel built and linked in a separate temporary directory using pinned local Circle-stdlib checkout; output `kernel8-32.img` = 16,822,164 bytes.
- Historical incremental v1.15 binary from working tree had size 16,817,460 bytes; the new kernel is a **source-build validation only**, not a byte-identical binary reproduction.
- No hardware boot or game compatibility test of the reconstructed binary was performed.
- The Circle-stdlib and Circle code is not vendored in this archive; external source revisions and license copies are documented in README and LICENSES.
- Licensing compatibility of MAME 0.106 non-commercial licence and GPLv3 Circle remains unresolved. This is a source export, **not authorization to distribute a linked binary or market this as a fully legally cleared product**.

After obtaining necessary legal permissions/exception, retest the binary on Pi 3 and verify the final distribution with all corresponding source.
