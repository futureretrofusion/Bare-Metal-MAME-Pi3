# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Independent Pi3 ARM32 diagnostic images: Stage 0 serial, Stage 1 mailbox FB.
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM := $(CROSS_COMPILE)nm
SIZE := $(CROSS_COMPILE)size
BUILD := build
COMMON := -mcpu=cortex-a53 -marm -mfloat-abi=soft -ffreestanding \
          -fno-builtin -fno-stack-protector -fno-unwind-tables \
          -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections
CFLAGS := $(COMMON) -std=c11 -O2 -Wall -Wextra -Werror
ASFLAGS := $(COMMON)
LDFLAGS := -nostdlib -nostartfiles -Wl,--gc-sections \
           -Wl,--build-id=none -T linker.ld
BOOT_OBJS := $(BUILD)/boot.o $(BUILD)/hal.o
STAGE0_OBJS := $(BOOT_OBJS) $(BUILD)/main.o
STAGE1_OBJS := $(BOOT_OBJS) $(BUILD)/mailbox.o $(BUILD)/framebuffer.o $(BUILD)/main_fb.o
STAGE2_OBJS := $(BOOT_OBJS) $(BUILD)/irq.o $(BUILD)/main_irq.o
USBPOWER_OBJS := $(BOOT_OBJS) $(BUILD)/mailbox.o $(BUILD)/usb_power.o $(BUILD)/main_usbpower.o

.PHONY: all stage1 stage2 usbpower clean check check-stage1 check-stage2 check-usbpower
all: $(BUILD)/frf-hal-stage0.img
stage1: $(BUILD)/frf-hal-stage1.img
stage2: $(BUILD)/frf-hal-stage2.img
usbpower: $(BUILD)/frf-hal-usbpower.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.S | $(BUILD)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BUILD)/%.o: %.c hal.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/frf-hal-stage0.elf: $(STAGE0_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-stage0.map -o $@ $(STAGE0_OBJS)

$(BUILD)/frf-hal-stage1.elf: $(STAGE1_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-stage1.map -o $@ $(STAGE1_OBJS)

$(BUILD)/frf-hal-stage2.elf: $(STAGE2_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-stage2.map -o $@ $(STAGE2_OBJS)

$(BUILD)/frf-hal-usbpower.elf: $(USBPOWER_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-usbpower.map -o $@ $(USBPOWER_OBJS)

$(BUILD)/frf-hal-stage0.img: $(BUILD)/frf-hal-stage0.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-stage0.elf

$(BUILD)/frf-hal-stage1.img: $(BUILD)/frf-hal-stage1.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-stage1.elf

$(BUILD)/frf-hal-stage2.img: $(BUILD)/frf-hal-stage2.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-stage2.elf

$(BUILD)/frf-hal-usbpower.img: $(BUILD)/frf-hal-usbpower.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-usbpower.elf

check: all
	$(OBJDUMP) -h $(BUILD)/frf-hal-stage0.elf
	$(NM) -u $(BUILD)/frf-hal-stage0.elf

check-stage1: stage1
	$(OBJDUMP) -h $(BUILD)/frf-hal-stage1.elf
	$(NM) -u $(BUILD)/frf-hal-stage1.elf

check-stage2: stage2
	$(OBJDUMP) -h $(BUILD)/frf-hal-stage2.elf
	$(NM) -u $(BUILD)/frf-hal-stage2.elf

check-usbpower: usbpower
	$(OBJDUMP) -h $(BUILD)/frf-hal-usbpower.elf
	$(NM) -u $(BUILD)/frf-hal-usbpower.elf

clean:
	rm -rf $(BUILD)

# Experimental isolated TinyUSB enumeration diagnostic (Stage 2 follow-on) (NO Circle).
# Deliberately separate from old circle-kernel and all MAME link targets.
TUSB_ROOT := ../third_party/frf_tinyusb_mame/src
TUSB_CFLAGS := $(COMMON) -std=c11 -O2 -I. -I$(TUSB_ROOT) -DFRF_USB_TINYUSB=1
USBHOST_OBJS := $(BOOT_OBJS) $(BUILD)/mailbox.o $(BUILD)/usb_power.o \
    $(BUILD)/usb_runtime.o $(BUILD)/main_usbhost.o $(BUILD)/input.o $(BUILD)/dma.o \
    $(BUILD)/tusb_core.o $(BUILD)/tusb_fifo.o $(BUILD)/tusb_usbh.o \
    $(BUILD)/tusb_hub.o $(BUILD)/tusb_hid_host.o \
    $(BUILD)/tusb_dwc2.o $(BUILD)/tusb_dwc2_common.o
.PHONY: usbhost check-usbhost
usbhost: $(BUILD)/frf-hal-usbhost.img
$(BUILD)/tusb_core.o: $(TUSB_ROOT)/tusb.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_fifo.o: $(TUSB_ROOT)/common/tusb_fifo.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_usbh.o: $(TUSB_ROOT)/host/usbh.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_hub.o: $(TUSB_ROOT)/host/hub.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_hid_host.o: $(TUSB_ROOT)/class/hid/hid_host.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_dwc2.o: $(TUSB_ROOT)/portable/synopsys/dwc2/hcd_dwc2.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/tusb_dwc2_common.o: $(TUSB_ROOT)/portable/synopsys/dwc2/dwc2_common.c tusb_config.h | $(BUILD)
	$(CC) $(TUSB_CFLAGS) -c -o $@ $<
$(BUILD)/main_usbhost.o: main_usbhost.c tusb_config.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I$(TUSB_ROOT) -DFRF_USB_TINYUSB=1 -c -o $@ $<
$(BUILD)/frf-hal-usbhost.elf: $(USBHOST_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-usbhost.map -o $@ $(USBHOST_OBJS) -lgcc
$(BUILD)/frf-hal-usbhost.img: $(BUILD)/frf-hal-usbhost.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-usbhost.elf
check-usbhost: usbhost
	$(OBJDUMP) -h $(BUILD)/frf-hal-usbhost.elf
	$(NM) -u $(BUILD)/frf-hal-usbhost.elf

# Isolated Pi3 Cortex-A53 ARM32 short-descriptor MMU/cache smoke test.
MMU_OBJS := $(BOOT_OBJS) $(BUILD)/mmu.o $(BUILD)/mmu_arm.o $(BUILD)/main_mmu.o
.PHONY: mmu check-mmu host-test-mmu
mmu: $(BUILD)/frf-hal-mmu.img
$(BUILD)/mmu_arm.o: mmu_arm.S mmu.h | $(BUILD)
	$(CC) $(ASFLAGS) -c -o $@ $<
$(BUILD)/frf-hal-mmu.elf: $(MMU_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-mmu.map -o $@ $(MMU_OBJS)
$(BUILD)/frf-hal-mmu.img: $(BUILD)/frf-hal-mmu.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-mmu.elf
check-mmu: mmu
	$(OBJDUMP) -h $(BUILD)/frf-hal-mmu.elf
	$(NM) -u $(BUILD)/frf-hal-mmu.elf
host-test-mmu: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -o $(BUILD)/host-test-mmu tests/test_mmu.c mmu.c
	$(BUILD)/host-test-mmu
HOSTCC ?= gcc

# Independent multicore firmware mailbox3 wakeup + per-core IPC diagnostic.
SMP_OBJS := $(BOOT_OBJS) $(BUILD)/smp.o $(BUILD)/main_smp.o
.PHONY: smp check-smp host-test-smp
smp: $(BUILD)/frf-hal-smp.img
$(BUILD)/frf-hal-smp.elf: $(SMP_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-smp.map -o $@ $(SMP_OBJS)
$(BUILD)/frf-hal-smp.img: $(BUILD)/frf-hal-smp.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-smp.elf
check-smp: smp
	$(OBJDUMP) -h $(BUILD)/frf-hal-smp.elf
	$(NM) -u $(BUILD)/frf-hal-smp.elf
host-test-smp: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -o $(BUILD)/host-test-smp tests/test_smp.c
	$(BUILD)/host-test-smp

# Independently compiled FRF HID descriptor decoder for standalone TinyUSB.
CXX := $(CROSS_COMPILE)g++
$(BUILD)/input.o: input.cpp input.h ../baremetal/tinyusb/frf_hid_gamepad.h | $(BUILD)
	$(CXX) $(COMMON) -std=c++17 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-use-cxa-atexit -c -o $@ $<
.PHONY: host-test-input
host-test-input: | $(BUILD)
	g++ -std=c++17 -O2 -Wall -Wextra -Werror -o $(BUILD)/host-test-input input.cpp tests/test_input.cpp
	$(BUILD)/host-test-input

# Standalone DMA address/cache-maintenance integration and host checks.
.PHONY: host-test-dma
host-test-dma: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -o $(BUILD)/host-test-dma tests/test_dma.c dma.c
	$(BUILD)/host-test-dma

# Video integration: original FRF renderer linked to independent framebuffer.
# The Circle screen implementation is not built or copied.
VIDEO_OBJS := $(BOOT_OBJS) $(BUILD)/usb_runtime.o $(BUILD)/mailbox.o \
    $(BUILD)/framebuffer.o $(BUILD)/bm_timer_hal.o $(BUILD)/bm_video.o \
    $(BUILD)/main_video.o
.PHONY: video check-video
video: $(BUILD)/frf-hal-video.img
$(BUILD)/bm_video.o: ../baremetal/bm_video.c ../baremetal/bm_platform.h | $(BUILD)
	$(CC) $(CFLAGS) -Wno-error=unused-variable -DFRF_MAME_FAST=1 -c -o $@ $<
$(BUILD)/frf-hal-video.elf: $(VIDEO_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-video.map -o $@ $(VIDEO_OBJS) -lgcc
$(BUILD)/frf-hal-video.img: $(BUILD)/frf-hal-video.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-video.elf
check-video: video
	$(OBJDUMP) -h $(BUILD)/frf-hal-video.elf
	$(NM) -u $(BUILD)/frf-hal-video.elf

# Shared, non-destructive GPU IRQ routing with timer1 diagnostic.
IRQROUTER_OBJS := $(BOOT_OBJS) $(BUILD)/irq_router.o $(BUILD)/main_irq_router.o
.PHONY: irq-router check-irq-router host-test-irq-router
irq-router: $(BUILD)/frf-hal-irq-router.img
$(BUILD)/frf-hal-irq-router.elf: $(IRQROUTER_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-irq-router.map -o $@ $(IRQROUTER_OBJS)
$(BUILD)/frf-hal-irq-router.img: $(BUILD)/frf-hal-irq-router.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-irq-router.elf
check-irq-router: irq-router
	$(OBJDUMP) -h $(BUILD)/frf-hal-irq-router.elf
	$(NM) -u $(BUILD)/frf-hal-irq-router.elf
host-test-irq-router: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -o $(BUILD)/host-test-irq-router tests/test_irq_router.c irq_router.c
	$(BUILD)/host-test-irq-router

# Independent read-only Pi3 SDHOST (not Arasan) + original FAT32 parser.
# Deliberately NOT combined with MAME, SD writer, Circle, HDMI or USB.
STORAGE_OBJS := $(BOOT_OBJS) $(BUILD)/usb_runtime.o $(BUILD)/sdhost.o \
    $(BUILD)/fat32.o $(BUILD)/rom_catalog.o $(BUILD)/file_pool.o $(BUILD)/main_storage.o
.PHONY: storage check-storage host-test-fat32 host-test-sdhost
storage: $(BUILD)/frf-hal-storage.img
$(BUILD)/frf-hal-storage.elf: $(STORAGE_OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-storage.map -o $@ $(STORAGE_OBJS) -lgcc
$(BUILD)/frf-hal-storage.img: $(BUILD)/frf-hal-storage.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-storage.elf
check-storage: storage
	$(OBJDUMP) -h $(BUILD)/frf-hal-storage.elf
	$(NM) -u $(BUILD)/frf-hal-storage.elf
host-test-fat32: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -o $(BUILD)/host-test-fat32 tests/test_fat32.c fat32.c
	$(BUILD)/host-test-fat32
host-test-sdhost: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -o $(BUILD)/host-test-sdhost tests/test_sdhost.c
	$(BUILD)/host-test-sdhost

# Native ROM-index and real formatted FAT32 image tests (scratch file only).
.PHONY: host-test-catalog host-test-fat32-image
host-test-catalog: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -o $(BUILD)/host-test-catalog tests/test_rom_catalog.c rom_catalog.c
	$(BUILD)/host-test-catalog
host-test-fat32-image: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -o $(BUILD)/host-test-fat32-image tests/test_fat32_image.c fat32.c rom_catalog.c
	@test -n "$(FRF_FAT32_IMAGE)" || { echo "Set FRF_FAT32_IMAGE to an existing disposable FAT32 image"; exit 2; }
	$(BUILD)/host-test-fat32-image $(FRF_FAT32_IMAGE)

# Real formatted FAT32 image: fixed-size interleaved ROM file-handle API.
.PHONY: host-test-file-pool
host-test-file-pool: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -o $(BUILD)/host-test-file-pool tests/test_file_pool.c fat32.c file_pool.c
	@test -n "$(FRF_FAT32_IMAGE)" || { echo "Set FRF_FAT32_IMAGE to a disposable FAT32 image"; exit 2; }
	$(BUILD)/host-test-file-pool $(FRF_FAT32_IMAGE)

# Full register-command state machine tested with deterministic SDHOST MMIO.
.PHONY: host-test-sdhost-mock
host-test-sdhost-mock: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -DFRF_SDHOST_MOCK -fsanitize=address,undefined -o $(BUILD)/host-test-sdhost-mock tests/test_sdhost_mock.c sdhost.c
	$(BUILD)/host-test-sdhost-mock
