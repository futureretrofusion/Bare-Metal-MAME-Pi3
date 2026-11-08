# Independent FRF HAL code and third-party provenance

Copyright © 2026 Future Retro Fusion.

This inventory is not a legal clearance. It identifies code actually
linked in the isolated diagnostic images.

| Component | Source | Disposition |
|---|---|---|
| New `frf-hal/*` boot, MMU, IRQ, SMP, DMA, UART, mailbox adapters | Original FRF independent 2026 code; `frf-hal/LICENSE` MIT | Used in diagnostics |
| `baremetal/bm_video.c` | Original FRF frontend code from v1.15 snapshot | Linked unchanged into video diagnostic; retain original notices and audit ownership |
| `baremetal/tinyusb/frf_hid_gamepad.h` | FRF generic parser in existing v1.15 source | Linked unchanged in new input adapter; provenance should be audited before release |
| `baremetal/tinyusb/frf_known_pads.h` | Header explicitly says adapted from EMU68AGA USPi `usbgamepad.c` | **EXCLUDED**: USPi upstream GPLv3 provenance risk |
| `third_party/frf_tinyusb_mame/` | TinyUSB upstream MIT, modified vendor port | Linked in USB diagnostic; audit all local deltas and references to USPi algorithms before release |
| Circle / Circle-stdlib / `screen_dbuf.cpp` | GPLv3 | **Never linked into FRF HAL** |
| `mame106/` | Historical noncommercial terms with per-file rights | Not linked into any new diagnostic; retain licensing before future link |

Hardware datasheets, public ARM and BCM register specifications are
the reference for independent FRF register-level routines. No
Circle screen/startup implementation was copied into `frf-hal/`.

**No public combined binary distribution yet.** Confirm file-by-file
provenance, the historical MAME noncommercial terms, FatFs permissions,
newlib exceptions, modified TinyUSB code and trademarks before any
MAME/FRF HAL release.
