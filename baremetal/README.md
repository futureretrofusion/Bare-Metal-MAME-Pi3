# Mame106RGB Bare Metal Pi3 v0.1

Base: known-working Mame106RGB V103-from-working-V102.

Architecture:
- CPU0: MAME 0.106 emulation/core loop.
- CPU1: RGB565 integer scaler.
- CPU2: scanline + framebuffer presentation.
- CPU3: reserved for audio/input/I/O worker.
- Video: firmware framebuffer, RGB565, integer-only scale, centered image.
- Scanlines: post-scale darkening on alternate physical output lines.

The Emu68 Pi3 AArch64 startup/framebuffer code is retained under donor/ as the hardware reference. The next integration layer replaces AmigaOS OSD services with bare-metal timer, framebuffer, input/audio and FAT/ROM services.
