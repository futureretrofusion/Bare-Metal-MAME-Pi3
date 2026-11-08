# FRF HAL Stage 0 — build verification (Studio Main)

Copyright © 2026 Future Retro Fusion.

## Baseline
- Source snapshot: v1.15 public source at commit `34783aeb96a672ff6018563b6c5736080faba7ec`.
- Development branch: `frf-independent-hal`.
- Isolated worktree: `/home/fff2025/Documents/Amiga/FRF-BareMetal-MAME-Pi3-FRF-HAL-DEV`.
- Production `main` worktree was verified clean after file creation.
- No production kernel or removable media was altered.

## Compiled on 8 November 2026
- ARM compiler: `arm-none-eabi-gcc 14.2.1`.
- Command: `make -C frf-hal check`.
- Result: **exit 0**, generated ELF, link map and ARM32 binary.
- Link address and entry: `0x8000`.
- Binary file size: **1,597 bytes**.
- ELF section sizes: text **1,597**, data **0**, BSS **21,520** bytes.
- `arm-none-eabi-nm -u build/frf-hal-stage0.elf`: **no undefined symbols**.
- Search of link map for Circle, CStdlib, libsound and newlib: **no matches**.
- Image SHA-256: `15f60b725afacafc550d5c62cd2497746b565eadf7a212fb0c83c51be7b0ebad`
- ELF SHA-256: `7d35f7dcc01ddd3b0082e3e71c908d108037fb2f5ee1db40aaeb6047761177c2`.

## NOT yet verified
- No Raspberry Pi 3 UART boot observations.
- No CPU exception injection/fault handler runtime test.
- No serial timing/baud/heartbeat measurement on physical hardware.
- `qemu-system-arm` is not installed on Studio Main.
- No SD, framebuffer, TinyUSB, multicore, HDMI audio or MAME linkage yet.
- Not a redistributable combined emulator binary. GPL removal from final
  MAME executable and all third-party licence issues remain to be verified.

## Hardware test acceptance
With a disposable Pi 3 FAT test boot partition and 3.3 V TTL serial adapter,
record the exact serial log demonstrating the entry banner, BSS PASS, timer
PASS and 1-second heartbeat, then test deliberate undefined-instruction
exception on a second, separately named test image. Do not mark Stage 0 as
hardware-passed until both logs are preserved.
