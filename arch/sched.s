@ Copyright (C) StrawberryHacker

.syntax unified
.cpu cortex-a5
.arm

MODE_MASK  = 0b11111
SYS_MODE   = 0b11111
USER_MODE  = 0b10000

.data
sched_panic_text:
    .asciz "Next thread is wrong"
sched_file_text:
    .asciz "/arch/sched.s"

.text

@ Called when the core scheduler gives rq->next equal NULL
sched_next_panic:
    mov r1, #8
    ldr r0, =sched_file_text
    ldr r2, =sched_panic_text
    b panic_handler

@ Make sched_start accessable from main.c
.global sched_start

@ Starts the scheduler. This functions assumes that the scheduler has been
@ called once so that the first thread to run is determined
@
@ void sched_start(void)
sched_start:

    cpsid if
    bl cpu_timer_start

    @ Run the core scheduler once to get the first thread to run
    ldr r0, =rq
    bl core_sched

    @ The context switch dones not run here This means we have to manually
    @ switch the rq->next and rq->curr so that rq->curr is pointing to the 
    @ current running thread and rq->next is pointing to NULL
    ldr r0, =rq
    ldr r1, [r0]      @ Load rq->next into r1

    @ If rq->next is NULL we have a core scheduler panic
    cmp r1, #0
    beq sched_next_panic
    mov r2, #0
    str r1, [r0, #4]
    str r2, [r0]

    @ Conditionally update the memory map
    ldr r2, [r1, #4]
    cmp r2, #0
    ldrne r0, [r2]
    mcrne p15, 0, r0, c2, c0, 0  @ Update the TTBR0 with the current memory map
    isb
    mcrne p15, 0, r0, c8, c7, 0  @ Flush the TLB / uTLB
    dsb
    isb

    @ Clean all the cache
    bl icache_invalidate
    bl dcache_clean
    
    @ Switch to SYS mode now to avoid returning to a banked LR_svc
    cps #SYS_MODE

    ldr r0, =rq
    ldr r1, [r0, #4]
    ldr sp, [r1]      @ Update SP_sys
    dsb
    isb

    @ Unstack the first threads stack frame
    ldmia sp!, {r4 - r11}
    add sp, sp, #4
    ldmia sp!, {lr}
    ldmia sp!, {r0 - r3, r12}

    rfeia sp!  @ Enables the interrupt due to initial CPSR (I and F flag)
