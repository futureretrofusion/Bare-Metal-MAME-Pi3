/* Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT */
/* Pi3 ARM32 firmware loads 32-bit kernel image at ARM physical 0x8000. */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
    . = 0x8000;
    __image_start__ = .;
    .text : ALIGN(32) { KEEP(*(.text.boot)) *(.text .text.*) }
    .rodata : ALIGN(4) { *(.rodata .rodata.*) }
    .data : ALIGN(4) { *(.data .data.*) }
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end__ = .;
    }
    __image_end__ = .;
    /DISCARD/ : { *(.comment) *(.note .note.*) *(.ARM.exidx .ARM.exidx.*) }
}
ASSERT(__image_end__ < 0x02000000, "FRF HAL stage0 exceeds reserved 32MB boot workspace")
