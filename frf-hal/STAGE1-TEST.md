# Stage 1 — optional VideoCore framebuffer diagnostic

Copyright © 2026 Future Retro Fusion. FRF HAL MIT code only.

`make -C frf-hal check-stage1` builds **frf-hal-stage1.img** as a
separate diagnostic. It must not be confused with or installed over the
existing MAME v1.15 production kernel. The original Stage-0 image is
still available using `make -C frf-hal check`.

Stage 1 uses the independent Pi3 property mailbox protocol on channel 8
with a 16-byte-aligned request buffer. It requests a 640×480 RGB565
framebuffer with virtual height 960 and checks that allocation,
pixel-order, pitch, image sizes and double-page virtual height have
been accepted. It draws a red/white/green test pattern on page 0 and
blue/white/green on page 1, alternating once a second using a mailbox
offset/VSync request. The original FRF GUI is **not yet connected**.

On an otherwise bootable, disposable Pi3 FAT test SD, keep the normal
firmware files, copy `frf-hal-stage1.img` to the FAT boot partition,
and configure:

```ini
arm_64bit=0
kernel=frf-hal-stage1.img
enable_uart=1
dtoverlay=disable-bt
init_uart_clock=48000000
```

Observe GPIO14 TXD0 physical pin 8 with a **3.3 V TTL** adapter at 115200
8N1, common ground. Never connect RS-232 +/-12 V or 5 V. A functioning
test should print the serial banner, timer check, mailbox framebuffer
allocation information and one page-flip report per second, while the
HDMI display alternates two full-screen colour patterns.

**Verification status:** compilation/linkage only. No real Pi 3
mailbox, HDMI display, VBlank timing or tearing observations have been
made. The mailbox transport is polled and single-core; it drops unrelated
channels rather than queueing messages. It cannot yet coexist safely with
other asynchronous VideoCore users. Framebuffer access assumes the ARM
MMU/data cache is disabled, as in this bootstrap.

Relevant hardware specifications:
- Raspberry Pi firmware wiki: `Mailbox property interface`
- Raspberry Pi firmware wiki: `Accessing mailboxes`
- BCM2835/BCM2837 mailbox register map and Pi3 peripheral remapping

There is no final binary release authorization at this milestone.
