# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Include with: make -C frf-hal -f Makefile -f gui.mk check-gui
# Existing FRF graphics only: excludes every Circle source/library.
GUI_OBJS := $(BOOT_OBJS) $(BUILD)/mailbox.o $(BUILD)/framebuffer.o \
    $(BUILD)/usb_runtime.o $(BUILD)/mini_runtime.o $(BUILD)/main_gui.o $(BUILD)/frf_gui.o
.PHONY: gui check-gui
gui: $(BUILD)/frf-hal-gui.img

$(BUILD)/frf_gui.o: ../circle-kernel/frf_gui.cpp ../circle-kernel/frf_gui.h ../circle-kernel/frf_gui_font.h ../circle-kernel/frf_gui_banner.h | $(BUILD)
	$(CXX) $(COMMON) -std=c++17 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-use-cxa-atexit -I../circle-kernel -c -o $@ $<

$(BUILD)/frf-hal-gui.elf: $(GUI_OBJS) linker.ld
	$(CXX) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-gui.map -o $@ $(GUI_OBJS) -lgcc

$(BUILD)/frf-hal-gui.img: $(BUILD)/frf-hal-gui.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-gui.elf

check-gui: gui
	$(OBJDUMP) -h $(BUILD)/frf-hal-gui.elf
	$(NM) -u $(BUILD)/frf-hal-gui.elf

# Limited native formatter regression. No framebuffer required.
HOSTCC ?= gcc
.PHONY: host-test-mini-runtime
host-test-mini-runtime: | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin -o $(BUILD)/host-test-mini-runtime tests/test_mini_runtime.c mini_runtime.c usb_runtime.c
	$(BUILD)/host-test-mini-runtime
