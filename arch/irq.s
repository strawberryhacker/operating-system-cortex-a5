@ Copyright (C) strawberryhacker

.syntax unified
.cpu cortex-a5
.arm

@ ARMv7-A modes in the CPSR
MODE_MASK  = 0b11111
USER_MODE  = 0b10000
FIQ_MODE   = 0b10001
IRQ_MODE   = 0b10010
SVC_MODE   = 0b10011
ABORT_MODE = 0b10111
UNDEF_MODE = 0b11011
SYS_MODE   = 0b11111

@ Physical address of secure / non-secure APIC
APIC_BASE  = 0xFC020000
SAPIC_BASE = 0xF803C000

@ Local offsets from APIC / SAPIC
APIC_IVR   = 0x10
APIC_FVR   = 0x14
APIC_EOICR = 0x38
APIC_CISR  = 0x34
APIC_SMR   = 0x04

@ The vector table is mapped to virtual address 0xFFFF0000
.section .vector_table, "ax", %progbits
vector_table:

    .word 0
    ldr pc, =__undef_exception      @ Undefined instruction
    ldr pc, =__supervisor_exception @ Supervisor
    ldr pc, =__prefetch_exception   @ Prefetch  abort
    ldr pc, =__data_exception       @ Data abort
    .word 0
    ldr pc, =__irq_exception        @ IRQ
    .word 0                         @ FIQ

@ In general; all interrupts disable IRQ and only reset and FIQ disables FIQ.
@ The IRQ flag is cleared in the IRQ exception in order to support nesting. In
@ the fault handlers this is not done. Therefore fault handlers can never be
@ nested

@ Undefined instruction exception
@
@ void __undef_exception(void)

.type __undef_exception, %function
__undef_exception:
    stmdb sp!, {r0 - r3, r12, lr}
    mov r0, lr
    bl undef_exception
    ldmia sp!, {r0 - r3, r12, lr}
    subs pc, lr, #4

@ CPU tries to execute a instruction marked as aborted whithin the pipeline
@
@ void __prefetch_exception(void)

.type __prefetch_exception, %function
__prefetch_exception:

    sub lr, lr, #4
    stmdb sp!, {r0 - r3, r12, lr}

    mov r0, lr              @ LR_abort will hold the PC after adjustment

    bl prefetch_exception
    ldmia sp!, {r0 - r3, r12, lr}
    movs pc, lr

@ Data abort exception
@
@ void __data_exception(void)

.type __data_exception, %function
__data_exception:
    sub lr, lr, #8
    stmdb sp!, {r0 - r3, r12, lr}

    mov r0, lr              @ LR_abort will hold the PC after adjustment

    bl data_exception
    ldmia sp!, {r0 - r3, r12, lr}
    movs pc, lr

@ Supervisor exception
@
@ void __supervisor_exception(void)

.type __supervisor_exception, %function
__supervisor_exception:

    srsfd sp!, #SYS_MODE       @ Stores LR_irq and SPSR_irq in SP_sys
    cps #SYS_MODE
    stmdb sp!, {r0 - r3, r12}  @ Store AAPCS registers on the kernel stack
    and r1, sp, #4
    sub sp, sp, r1
    stmdb sp!, {r1, lr}        @ Push the padding and the real LR

    mov r0, sp
    bl supervisor_exception

    b context_core             @ Check if we are going to do a context switch

@ IRQ exception handler. This will handle both secure and non-secure interrupts
@ and the context switch if the next_thread is set be the scheduler
@
@ void __irq_exception(void)

.type __irq_exception, %function
__irq_exception:

    sub lr, lr, #4             @ TODO: Add support for Thumb mode
    srsfd sp!, #SYS_MODE       @ Stores LR_irq and CPSR_irq in SP_sys
    cps #SYS_MODE

    stmdb sp!, {r0 - r3, r12}  @ Store AAPCS registers on the SP_sys

    @ The AAPCS for ABI requires the SP to be aligned with 8 bytes due to 
    @ maximizing the 64-bit AXI matrix performance. The SP is always word 
    @ aligned so we only need to care about bit 3
    and r1, sp, #4
    sub sp, sp, r1
    stmdb sp!, {r1, lr}         @ Push the padding and the real LR

    ldr r1, =APIC_BASE
    ldr r0, [r1, #APIC_IVR]     @ Get the interrupt source from the APIC
    str r1, [r1, #APIC_IVR]
    ldr r1, [r1, #APIC_SMR]     @ To avoid crash

    cpsie i
    blx r0                      @ Branch to APIC interrupt handler
    cpsid i

    ldr r0, =APIC_BASE
    str r0, [r0, #APIC_EOICR]

context_core:
    @ If rq->next is non-zero we execute the context switch
    ldr r0, =rq
    ldr r1, [r0]                @ Addr of the next thread to run
    cmp r1, #0
    beq skip_context

    stmdb sp!, {r4 - r11}

    ldr r3, [r0, #4]
    str sp, [r3]                @ Store the SP in the curr_thread_sp

    @ Switch the memory map in case of user threads
    ldr r4, [r1, #4]
    cmp r4, #0
    ldrne r3, [r4]              @ Get the base address of the next memory map
    mcrne p15, 0, r3, c2, c0, 0
    isb
    mcrne p15, 0, r0, c8, c7, 0 @ Flush the entire TLB / uTLB
    dsb
    isb

    @ Set rq->curr to rq->next, and rq->next to NULL
    str r1, [r0, #4]
    mov r2, #0
    str r2, [r0]
    
    ldr sp, [r1]                @ Load the new stack pointer
    dmb
    isb

    ldmia sp!, {r4 - r11}

    @ Disable the FPU because we support lazy context switch
    vmrs r0, fpexc
    bic r0, r0, #(1 << 30)
    vmsr fpexc, r0

skip_context:
    ldmia sp!, {r1, lr}
    add sp, sp, r1

    ldmia sp!, {r0 - r3, r12}   @ Pop the AAPCS registers
    rfefd sp!                   @ Return from exception
    