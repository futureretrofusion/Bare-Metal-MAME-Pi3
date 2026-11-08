# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Include with: make -C frf-hal -f Makefile -f gui.mk -f rommenu.mk check-rommenu
# Existing FRF GUI + independently implemented readonly SDHOST and FAT32.
ROMMENU_OBJS := $(BOOT_OBJS) $(BUILD)/mailbox.o $(BUILD)/framebuffer.o \
    $(BUILD)/usb_runtime.o $(BUILD)/mini_runtime.o $(BUILD)/frf_gui.o \
    $(BUILD)/sdhost.o $(BUILD)/fat32.o $(BUILD)/main_rommenu.o

.PHONY: rommenu check-rommenu
rommenu: $(BUILD)/frf-hal-rommenu.img
$(BUILD)/frf-hal-rommenu.elf: $(ROMMENU_OBJS) linker.ld
	$(CXX) $(COMMON) $(LDFLAGS) -Wl,-Map,$(BUILD)/frf-hal-rommenu.map -o $@ $(ROMMENU_OBJS) -lgcc
$(BUILD)/frf-hal-rommenu.img: $(BUILD)/frf-hal-rommenu.elf
	$(OBJCOPY) -O binary $< $@
	$(SIZE) $(BUILD)/frf-hal-rommenu.elf
check-rommenu: rommenu
	$(OBJDUMP) -h $(BUILD)/frf-hal-rommenu.elf
	$(NM) -u $(BUILD)/frf-hal-rommenu.elf
