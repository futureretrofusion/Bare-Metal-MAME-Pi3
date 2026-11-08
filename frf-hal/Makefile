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
    $(BUILD)/usb_runtime.o $(BUILD)/main_usbhost.o \
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
