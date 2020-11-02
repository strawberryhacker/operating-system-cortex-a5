/* Copyright (C) strawberryhacker */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm","elf32-littlearm")
OUTPUT_ARCH(arm)
SEARCH_DIR(.)
ENTRY(entry)

MEMORY {
    mem (rwx) : ORIGIN = 0x00100000, LENGTH = 128k
}

SECTIONS {
    /* Text */
    .text : {
        . = ALIGN(4);
        _text_s = .;
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        
        . = ALIGN(4);
        
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _text_e = .;
    } > mem

    /* Black C magic */
    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > mem

    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > mem

    .preinit_array : {
        KEEP(*(.preinit_array))
    } > mem

    .init_array : {
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
    } > mem

    .fini_array : {
        KEEP(*(.fini_array))
		KEEP(*(SORT(.fini_array.*)))
    } > mem

    /* Constant data */
    .rodata : {
        . = ALIGN(4);
        _rodata_s = .;
        KEEP(*(.rodata))
        KEEP(*(.rodata*))
        . = ALIGN(4);
        _rodata_e = .;
    } > mem

    /* Initialized data and ram-functions */
    .data : {
        . = ALIGN(4);
        _data_s = .;
        KEEP(*(.data))
        KEEP(*(.data*))
        KEEP(*(.ramfunc))
        KEEP(*(.ramfunc*))
        . = ALIGN(4);
        _data_e = .;
    } > mem

    /* Uninitialized data */
    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_s = .;
        *(.bss)
        *(.bss*)
        . = ALIGN(4);
        _bss_e = .; 
    } > mem
}

_end = .;
