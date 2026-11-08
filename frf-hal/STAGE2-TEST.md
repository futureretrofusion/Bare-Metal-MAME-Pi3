# FRF HAL Stage 2 — ARM interrupt controller + system timer

Copyright © 2026 Future Retro Fusion; independently authored MIT FRF HAL code.

Build on Studio Main:

```sh
make -C frf-hal check-stage2
```

This builds the separate `frf-hal-stage2.img` with the previously
independent boot, MMIO, UART, exception vectors, and a new interrupt
handler which saves AAPCS scratch registers and returns with
`subs pc, lr, #4`. It masks other IRQ sources and arms BCM2837
System Timer compare **channel 1** at an approximately 10 ms cadence.
The main loop samples IRQ ticks every second via an independent polled
system-timer delay. A typical successful log would show 80–120 ticks
per second, zero spurious IRQs and a PASS indicator.

No Circle, Circle-stdlib, USB, MAME or linked C library is needed for
this test. It uses a dedicated test image because it **disables all
other ARM IRQ sources**; it must not be linked as-is into full MAME.

Hardware testing requires a disposable, bootable Pi 3 FAT card and
3.3 V TTL serial adapter. `config.txt` example:

```ini
arm_64bit=0
kernel=frf-hal-stage2.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

Expected UART output is not evidence of a pass until captured from
real hardware. **Studio Main compile/link results are the only
verification until a real Pi 3 serial log is supplied.**

The interrupt controller path must later be expanded into
per-source interrupt registration, a safe scheduler, and multimode
handling without globally disabling other IRQs.
