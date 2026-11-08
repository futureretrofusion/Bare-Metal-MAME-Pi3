# FRF HAL standalone PCM queue — build verification

Copyright © 2026 Future Retro Fusion. The audio queue and test harness
are independently authored MIT-licensed FRF HAL components.

## Verified on Studio Main (8 November 2026)

The software PCM queue is an 8,192-frame stereo SPSC buffer. It accepts
48 kHz input or performs continuous linear 32 kHz -> 48 kHz conversion.
It tracks accepted, dropped, silent and played frames. The diagnostic
creates synthetic PCM only and discards it without a hardware sink.

```sh
make -C frf-hal host-test-audio check-audio
make -C frf-hal -f Makefile -f audio-tests.mk host-test-audio-threads
make -C frf-hal -f abi-hard.mk -f Makefile check-audio
```

All four commands completed successfully (ARM32 builds, native unit and
cross-thread tests). Host unit cases cover stream chunk boundaries,
resampling continuity, underflow silence, capacity overflow and
ring-buffer wraparound. Cross-thread producer/consumer delivered 32,768
stereo frames with no reported corruption or drops.

The ARM32 soft-float `frf-hal-audio.img` and independent hard-float
`build-hard/frf-hal-audio.img` compiled and linked with no unresolved
symbols. The hard-float build matches the MAME ABIv7 VFP-register calling
convention; it does **not** imply a complete MAME application link.

## Explicit hardware limitations

No GPIO, I2S, PWM, VCHIQ or HDMI audio driver is attached. No device is
configured to play the synthetic samples. **No sound has been heard or
tested on a physical Pi 3**; the code has not fixed OutRun's missing
YM2151 music or demonstrated full gameplay timing. HDMI hardware sound,
multicore cache-coherent SPSC scheduling and FRF/MAME OSD audio bridging
are still open engineering milestones. Preserve original Circle build
until independently hardware-proven.
