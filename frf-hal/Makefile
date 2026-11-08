# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Independent ARM32 diagnostic image. No Circle, Circle-stdlib or MAME linked.
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
           -Wl,-Map,$(BUILD)/frf-hal-stage0.map \
           -Wl,--build-id=none -T linker.ld
OBJS := $(BUILD)/boot.o $(BUILD)/hal.o $(BUILD)/main.o

.PHONY: all clean check
all: $(BUILD)/frf-hal-stage0.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.S | $(BUILD)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BUILD)/hal.o: hal.c hal.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/main.o: main.c hal.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/frf-hal-stage0.elf: $(OBJS) linker.ld
	$(CC) $(COMMON) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/frf-hal-stage0.img: $(BUILD)/frf-hal-stage0.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-stage0.elf

check: all
	$(OBJDUMP) -h $(BUILD)/frf-hal-stage0.elf
	$(NM) $(BUILD)/frf-hal-stage0.elf

clean:
	rm -rf $(BUILD)
