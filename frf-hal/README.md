# FRF HAL — Pi 3 independent bootstrap (Stage 0)

Copyright © 2026 Future Retro Fusion. FRF HAL files in this directory
are independently written and licensed under the MIT licence in `LICENSE`.
This licence applies **only** to new FRF HAL code, not to MAME 0.106,
third-party code, or the existing Circle-based files elsewhere in this repo.

## Scope
First isolated bring-up image for a BCM2837-based Raspberry Pi 3 in ARM32
mode. It has its own startup, BSS clearing, stack initialization, exception
vectors, PL011 UART driver, and 1 MHz system timer. It deliberately does not
link Circle, Circle-stdlib, MAME, USB, SD/FAT, framebuffer, audio or workers.

## Build (Studio Main)
```sh
make -C frf-hal check
```
Output: `frf-hal/build/frf-hal-stage0.img`. This is a **diagnostic image**,
not a game-capable binary release.

## Raspberry Pi 3 hardware test (only on a disposable test SD)
Use separate SD media, **never** overwrite v1.15 or the production SD.
Keep the Raspberry Pi firmware boot files on the test SD. Copy the stage-0
image to its FAT boot partition and add these lines to its test config.txt:

```ini
arm_64bit=0
kernel=frf-hal-stage0.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

Connect a **3.3 V TTL USB serial adapter**, not RS-232 voltage:
GPIO14 TXD0 at header pin 8 -> adapter RX, pin 6 -> ground.
Monitor at 115200 8N1. Avoid connecting adapter 5 V.
Expected serial text: banner, BSS PASS, timer PASS, then a heartbeat
approximately every second. These are *expected* results, not hardware
verification; no actual Pi3 boot has yet been observed.

The 32-bit load/link address is 0x8000. Only core 0 executes Stage 0;
other cores remain parked. Firmware initializes RAM; Stage 0 clears its
own BSS and establishes stacks but does **not** implement MMU/cache,
malloc, multicore release, interrupts, exception recovery or SD mounting.
The UART baud divisor assumes firmware honours init_uart_clock=48000000.

Do not distribute a combined MAME binary merely because this test works.
See `../BINARY-RELEASE-LICENSING.md` for remaining obligations.

## Additional isolated diagnostics

The same independent ARM32 runtime also supports `make -C frf-hal check-stage1`
for a polled VideoCore mailbox, RGB565 framebuffer and double-page test,
and `make -C frf-hal check-stage2` for a 100Hz system-timer IRQ smoke
test. These are separate images; the original Stage-0 image is preserved.
See `STAGE1-TEST.md` and `STAGE2-TEST.md`. All stages are compiled,
but none is yet certified booting on a physical Pi 3.
