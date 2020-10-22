/* Copyright (C) strawberryhacker */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm","elf32-littlearm")
OUTPUT_ARCH(arm)
SEARCH_DIR(.)

MEMORY {
    vddr (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

/*
 * Stacks sizes in each mode. Note: the kernel will run the scheduler in IRQ 
 * mode, the kernel will run in SVC mode, the FIQ is not used atm, the ABORT
 * and UNDEF is used for error handlings and page faults, the SYS mode is for
 * kernel threads and processes, and the USER mode is not used. This is because
 * each process and thread will allocate its own stack
 */
USER_STACK  = 512;
FIQ_STACK   = 512;
IRQ_STACK   = 512;
ABORT_STACK = 512;
SVC_STACK   = 512;
UNDEF_STACK = 512;

SECTIONS {

    /* 
     * The kernel entry is places in the beginning of the kernel. The bootloader
     * (c-boot) will load the kernel into DDR at some address and start
     * executing the first instruction in the binary. This instructions is
     * placed in this section, together will the PIC code that sets up the 
     * kernel
     */
    .kernel_entry : {
        _kernel_s = .;
        KEEP(*(.kernel_entry))
        . = ALIGN(4);
    } > vddr

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
    } > vddr

    /* Black C magic */
    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > vddr

    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > vddr

    .preinit_array : {
        KEEP(*(.preinit_array))
    } > vddr

    .init_array : {
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
    } > vddr

    .fini_array : {
        KEEP(*(.fini_array))
		KEEP(*(SORT(.fini_array.*)))
    } > vddr

    /* Constant data */
    .rodata : {
        . = ALIGN(4);
        _rodata_s = .;
        KEEP(*(.rodata))
        KEEP(*(.rodata*))
        . = ALIGN(4);
        _rodata_e = .;
    } > vddr

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
    } > vddr

    /*
     * The vector table has to be aligned with 4k because it will be remapped 
     * by the L2 page table to address 0xFFFF0000 (three low nibble is zero)
     */
    .vector_table : {
        . = ALIGN(4096);
        _vectors_s = .;
        KEEP(*(.vector_table))
        . = ALIGN(4);
    } > vddr

    /* Stacks in DDR */
    .stack (NOLOAD) : {

        . = ALIGN(8);

        /* User stack */
        _user_stack_s = .;
        . += USER_STACK;
        . = ALIGN(8);
        _user_stack_e = .;

        /* FIQ stack */
        _fiq_stack_s = .;
        . += FIQ_STACK;
        . = ALIGN(8);
        _fiq_stack_e = .;

        /* IRQ stack */
        _irq_stack_s = .;
        . += IRQ_STACK;
        . = ALIGN(8);
        _irq_stack_e = .;

        /* Abbort stack */
        _abort_stack_s = .;
        . += ABORT_STACK;
        . = ALIGN(8);
        _abort_stack_e = .;

        /* SVC stack */
        _svc_stack_s = .;
        . += SVC_STACK;
        . = ALIGN(8);
        _svc_stack_e = .;

        /* Undef stack */
        _undef_stack_s = .;
        . += UNDEF_STACK;
        . = ALIGN(8);
        _undef_stack_e = .;
    } > vddr

    /* Uninitialized data */
    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_s = .;
        *(.bss)
        *(.bss*)
        . = ALIGN(4);
        _bss_e = .; 
    } > vddr

    /* 
     * Early secondary page table for the kernel vectors; relocated to virtual
     * address 0xFFFF0000
     */
    .early_kernel_pt2 (NOLOAD) : {
        . = ALIGN(1024);
        _early_kernel_lv2_pt_s = .;
        . += 1024;
    } > vddr

    /* Early page table for the kernel */
    .early_kernel_pt1 (NOLOAD) : {
        . = ALIGN(16384);
        _early_kernel_lv1_pt_s = .;
        . += 16384;
    } > vddr

    /*
     * This marks the end of the kernel. The next section is just used in the 
     * transition between physical and virtual memory view. This marks the start
     * of the early boot_alloc which will overwrite the early user page table
     */
    _kernel_e = .;

    /* 
     * This maps physical DDR @ 0x20000000 into virtual memory @ 0x20000000
     * due to speculative pysical instrucions in the pipeline. This is the
     * user page table so we use the TTBR0 which means it can be alligned with
     * 8k
     */
    .early_usr_pt1 (NOLOAD) : {
        . = ALIGN(8192);
        _early_usr_lv1_pt_s = .;
        . += 8192;
    } > vddr    
}

_kernel_size = _kernel_e - _kernel_s;
_end = .;
