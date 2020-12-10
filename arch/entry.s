@ Copyright (C) StrawberryHacker

.syntax unified
.cpu cortex-a5
.arm

DDR_START = 0x20000000
DDR_SIZE  = 0x8000000

@ ARMv7-A modes
MODE_MASK  = 0b11111
USER_MODE  = 0b10000
FIQ_MODE   = 0b10001
IRQ_MODE   = 0b10010
SVC_MODE   = 0b10011
ABORT_MODE = 0b10111
UNDEF_MODE = 0b11011
SYS_MODE   = 0b11111

.extern _user_stack_e
.extern _fiq_stack_e
.extern _irq_stack_e
.extern _abort_stack_e
.extern _svc_stack_e
.extern _undef_stack_e

.extern _vectors_s
.extern _kernel_s

@ Maps the kernel virtual address to physical address. The vaddr register will
@ be remapped to the physical address. The offset is the kernel VA - PA offset.  
@ This offset is calculated with parameters given from the bootloader.
.macro paddr vaddr, offset
    sub \vaddr, \vaddr, \offset
.endm

@ Early kernel and user page tables
.extern _early_kernel_lv2_pt_s
.extern _early_kernel_lv1_pt_s
.extern _early_usr_lv1_pt_s

@ Entry point for the kernel. This contains the first instructions which will 
@ run on the system. The load address does not match the link address. Therefore
@ the entire code needs to be PIC before enabling the MMU. This entry code sets
@ up three page tables. 
@
@  - One kernel L1 page table where the DDR, IO space and vectors are mapped in
@  - One kernel L2 page talbe for mapping vectors to 0xFFFF0000
@  - One user L1 page table with identity mapping due to pipeline issues

.section .kernel_entry, "ax", %progbits
kernel_entry:

    cpsid fia

    @ Reset all registers except the r0, PC and CPSR. The CPSR should hold the 
    @ value 0x1df; system mode and I, F, A bits set
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    mov r12, #0
    mov r13, #0
    mov r14, #0

    @ r0 is holding the physical load address of the kernel. r0 will be used 
    @ throughout this code in order to convert between kernel logical and 
    @ physical addresses
    ldr r1, =_kernel_s
    sub r0, r1, r0
    
    @ Clear the kernel L1 page table
    ldr r1, =_early_kernel_lv1_pt_s
    paddr r1, r0               @ Phys addr of the kernel L1 page table
    mov r2, #4096
    mov r3, #0
1:  
    str r3, [r1], #4
    subs r2, r2, #1
    bne 1b

    @ Clear the kernel L2 page table
    ldr r1, =_early_kernel_lv2_pt_s
    paddr r1, r0               @ Phys addr of the kernel L2 page table
    mov r2, #256
    mov r3, #0
1:  
    str r3, [r1], #4
    subs r2, r2, #1
    bne 1b

    @ Clear the user L1 page table
    ldr r1, =_early_usr_lv1_pt_s
    paddr r1, r0               @ Phys addr of the user page table
    mov r2, #2048
    mov r3, #0
1:  
    str r3, [r1], #4
    subs r2, r2, #1
    bne 1b

    @ PT entry mask in r1
    mov r1, #(0b10 << 0)       @ Section entry
    orr r1, r1, #(0b11 << 2)   @ Cacheable and bufferable
    orr r1, r1, #(15 << 5)     @ PT domain 15 (kernel)
    orr r1, r1, #(0b11 << 10)  @ AP bits - full access

    @ Map the entire physical memory into the top 8 KiB of the kernel L1 page
    @ table, and set up the 1:1 temporarily map betweeen pysical DDR and early
    @ user L1 page table
    ldr r3, =_early_kernel_lv1_pt_s
    paddr r3, r0
    mov r2, #2048
    add r2, r3, r2, LSL #2     @ Pointer to PT entry @ 0x80000000 in kernel LV1 PT
    
    ldr r8, =_early_usr_lv1_pt_s
    paddr r8, r0
    mov r6, #512
    add r6, r8, r6, LSL #2     @ Pointer to PT entry @ 0x20000000 in user LV1 PT
    
    mov r3, #512               @ The DDR offset whichin the physical memory
    mov r4, #128               @ 128MB of DDR rsulting in 128 (0x80) PT entries
1:  
    mov r5, r3, LSL #20
    orr r5, r5, r1
    str r5, [r2], #4
    str r5, [r6], #4
    add r3, r3, #1
    subs r4, r4, #1
    bne 1b

    @ Deprecated ?
    @ Map internal SRAM @ 0x00000000 to VA 0x00000000. This assumes that the 
    @ AXI ROM relocation has been performed by the bootloader. In general, if 
    @ the bootloader uses interrupt; this is done
    ldr r2, =_early_usr_lv1_pt_s
    paddr r2, r0
    str r1, [r2]              @ Map 1 MB of internal memory

    @ Map IO space pages (1M each); 0xF00 0xF80 0xFC0
    ldr r2, =_early_kernel_lv1_pt_s
    paddr r2, r0
    bic r1, r1, #(0b11 << 2)  @ New page table mask for strongly ordered memory
    mov r3, #0xF00
    mov r4, r3, LSL #20
    orr r4, r4, r1
    str r4, [r2, r3, LSL #2]

    mov r3, #0xF80
    mov r4, r3, LSL #20
    orr r4, r4, r1
    str r4, [r2, r3, LSL #2]

    mov r3, #0xFC0
    mov r4, r3, LSL #20
    orr r4, r4, r1
    str r4, [r2, r3, LSL #2]

    @ Map in MMC IO regions
    mov r3, #0xA00
    mov r4, r3, LSL #20
    orr r4, r4, r1
    str r4, [r2, r3, LSL #2]

    mov r3, #0xB00
    mov r4, r3, LSL #20
    orr r4, r4, r1
    str r4, [r2, r3, LSL #2]

    @ We want to map in vectors in virtual DDR into the high vector fetch address
    @ at address 0xFFFF0000. We are using 2 levels of page tables because the 
    @ first level page table requires the page to be 1M aligned, thus increasing
    @ the binary size to > 1M
    ldr r1, =_early_kernel_lv2_pt_s
    paddr r1, r0
    orr r1, #0b01              @ Attribute for L2 PT pointer
    orr r1, #(15 << 5)         @ Setting domain 15 for vectors
    ldr r2, =_early_kernel_lv1_pt_s
    paddr r2, r0
    movw r3, #0xFFF
    str r1, [r2, r3, LSL #2]

    @ Map in the vector phys addr in the kernel LV2 page table
    ldr r1, =_early_kernel_lv2_pt_s
    paddr r1, r0
    mov r2, #0xF0
    ldr r3, =_vectors_s
    paddr r3, r0
    orr r3, #0b111110          @ Full access, cacheable, bufferable
    str r3, [r1, r2, LSL #2]

    @ Remap the virtual memory map so that there is a 2GB:2GB split. The TTBR0
    @ hold the base address of the lowest region (application) (8k PT). The 
    @ TTBR1 hold the base of the upper 2GB (8k PT)
    @ 0x0 - 0x7FFFFFFF
    @ This should be done before writing to the TTBR0 and TTBR1
    mov r1, #1                 @ N value
    mcr p15, 0, r1, c2, c0, 2
    dsb

    ldr r1, =_early_usr_lv1_pt_s
    paddr r1, r0
    mcr p15, 0, r1, c2, c0, 0  @ Update the PT base address register TTBR0

    @ The TTBR1 page table MUST be aligned with 16KiB
    ldr r1, =_early_kernel_lv1_pt_s
    paddr r1, r0
    mcr p15, 0, r1, c2, c0, 1  @ Update the PT base address register TTBR1
    dsb

    mov r1, #0b11
    lsl r1, r1, #30
    ldr r1, =0xFFFFFFFF
    mcr p15, 0, r1, c3, c0, 0  @ Enable the domain 15
    mcr p15, 0, r0, c8, c7, 0  @ Invalidate the TLB cache (and uTLB)  

    mrc p15, 0, r1, c1, c0, 0
    orr r1, r1, #1
    mcr p15, 0, r1, c1, c0, 0  @ Enable the MMU

    @ Clear the .bss
    ldr r1, =_bss_s
    ldr r2, =_bss_e
    mov r3, #0
1:  
    cmp r1, r2
    strne r3, [r1], #4
    bne 1b
    
    mrc p15, 0, r1, c1, c0, 0
    orr r1, #(1 << 13)
    mcr p15, 0, r1, c1, c0, 0  @ Remap the vector fetch address

    @ Setup stacks from DDR
    @ Reinititialize the kernel stacks with kernel virtual stack pointers
    mrs r3, cpsr
    bic r3, r3, #MODE_MASK
    orr r3, r3, #UNDEF_MODE
    msr cpsr_c, r3
    ldr sp, =_undef_stack_e

    bic r3, r3, #MODE_MASK
    orr r3, r3, #ABORT_MODE
    msr cpsr_c, r3
    ldr sp, =_abort_stack_e

    bic r3, r3, #MODE_MASK
    orr r3, r3, #IRQ_MODE
    msr cpsr_c, r3
    ldr sp, =_irq_stack_e

    bic r3, r3, #MODE_MASK
    orr r3, r3, #FIQ_MODE
    msr cpsr_c, r3
    ldr sp, =_fiq_stack_e

    bic r3, r3, #MODE_MASK
    orr r3, r3, #SYS_MODE
    msr cpsr_c, r3
    ldr sp, =_user_stack_e

    bic r3, r3, #MODE_MASK
    orr r3, r3, #SVC_MODE
    msr cpsr_c, r3
    ldr sp, =_svc_stack_e

    dmb
    isb

    @ Workaround for long jump (0x2000xxxx -> 0x8000xxxx). The MMU is set up so
    @ this does not affect program flow
    ldr r0, =0x60000000
    add pc, pc, r0

    bl __libc_init_array
    bl main
