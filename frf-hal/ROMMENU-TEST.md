# FRF HAL FAT32-ROM directory menu diagnostic

Copyright © 2026 Future Retro Fusion (independent MIT HAL components).
This target compiles the original project-authored `frf_gui.cpp` and FRF
font/banner *without* compiling the GPL-covered Circle screen code.

Build on Studio Main:
```sh
make -C frf-hal -f Makefile -f gui.mk -f rommenu.mk check-rommenu
```

Expected output: `frf-hal/build/frf-hal-rommenu.img` plus ELF/link map.
Load this file by its unique name onto a **disposable** FAT boot partition
with ordinary Raspberry Pi firmware files, not the production SD card.

```ini
arm_64bit=0
kernel=frf-hal-rommenu.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

The program first requests a 1280x720 RGB565 double-height framebuffer,
draws the original FRF GUI with an initial diagnostic message, initializes
the independent SDHOST driver, and mounts a supported FAT32 volume.
It scans the root `/roms` directory and counts ZIP filenames, displaying
up to the first 14 found, cycling a simulated selection highlight.

This image does NOT run MAME, decode any ROM, load CHD files, enumerate
gamepads, or play sound. It does not alter on-card contents. Its FAT32
reader only supports MBR FAT32 or partitionless FAT32 with ASCII filenames;
FAT12/16, exFAT, GPT and full Unicode are not supported. Directories
outside `/roms` are not scanned. The first 14 directory-order filenames
are shown, not an alphabetical or correctness-verified game index.

Hardware status remains **UNTESTED** until actual Pi 3 UART logs, a
framebuffer photograph, and a read-only ROM directory comparison exist.
A successful compile/link is not a hardware boot test.
