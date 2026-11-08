# Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
# Select a completely separate hard-float output tree for MAME ABI testing.
# Supply this makefile FIRST, so make expands every target with BUILD=build-hard.
# Example:
# make -C frf-hal -f abi-hard.mk -f Makefile -f gui.mk check-gui
# Cortex-A53 ARM32 hard-float/VFP ABI matches the existing MAME CMake toolchain.
override BUILD := build-hard
override COMMON := -DFRF_HAL_HARD_FLOAT=1 -mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -ffunction-sections -fdata-sections
