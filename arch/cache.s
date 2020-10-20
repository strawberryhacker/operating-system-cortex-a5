@ Copyright (C) StrawberryHacker

.syntax unified
.cpu cortex-a5
.arm

@ This functions invalidates and enables the instruction cache
@
@ void icache_enable(void)

.global icache_enable
.type icache_enable, %function
icache_enable:

    stmdb sp!, {lr}
    bl icache_invalidate
    ldmia sp!, {lr} 
    
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #(1 << 12)
    mcr p15, 0, r0, c1, c0, 0    @ Enable the I-cache
     
    bx lr

@ Disables the instructions cache and invalidates it.
@
@ void icache_disable(void)

.global icache_disable
.type icache_disable, %function
icache_disable:
    
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #(1 << 12)
    mcr p15, 0, r0, c1, c0, 0    @ Disable the I-cache

    stmdb sp!, {lr}
    bl icache_invalidate
    ldmia sp!, {lr}

    bx lr

@ Invalidated all instruction caches to PoU and flushed the target branch cache
@
@ void icache_invalidate(void)

.global icache_invalidate
.type icache_invalidate, %function
icache_invalidate:

    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0
    dsb
    isb
    bx lr

@ Enables and invalidates the L1 data cache
@
@ void dcache_enable(void)

.global dcache_enable
.type dcache_enable, %function
dcache_enable:

    stmdb sp!, {lr}
    bl dcache_invalidate
    ldmia sp!, {lr}

    mrc p15, 0, r0, c1, c0, 0
    orr r0, #(1 << 2)
    mcr p15, 0, r0, c1, c0, 0   @ Enable the D-cache

    bx lr

@ Cleans and disables the entire L1 data cache
@
@ void dcache_disable(void)

.global dcache_disable
.type dcache_disable, %function
dcache_disable:

    stmdb sp!, {lr}
    bl dcache_clean
    stmia sp!, {lr}

    mrc p15, 0, r0, c1, c0, 0
    bic r0, #(1 << 2)
    mcr p15, 0, r0, c1, c0, 0   @ Disable the D-cache

    bx lr

@ Cleans the entire L1 data cache
@
@ void dcache_clean(void)

.global dcache_clean
.type dcache_clean, %function
dcache_clean:

    mov r0, #0                  @ Way index
1:  
    mov r1, #0                  @ Set index
2:  
    mov r2, #0
    orr r2, r2, r1, LSL #5
    orr r2, r2, r0, LSL #30
    mcr p15, 0, r2, c7, c10, 2
    add r1, r1, #1
    cmp r1, #256
    bne 2b
    add r0, r0, #1
    cmp r0, #4
    bne 1b
    dsb
    isb
    bx lr

@ Taking in the start address in r0, and the end address in r1 and cleans the 
@ data cache between these two virtual addresses

.global dcache_clean_range
.type dcache_clean_range, %function
dcache_clean_range:

    bic r0, r0, #31              @ Align the start address by a cache line
1:  
    mcr p15, 0, r0, c7, c10, 1
    add r0, r0, #32
    cmp r0, r1
    blt 1b
    bx lr

@ Invalidates the entrie L1 data cache
@
@ void dcache_invalidate(void)

.global dcache_invalidate
.type dcache_invalidate, %function
dcache_invalidate:

    mov r0, #0               @ Way index - from 0 to 3
1:  
    mov r1, #0               @ Set index - from 0 to 255
2:  
    mov r2, #0
    orr r2, r2, r1, LSL #5
    orr r2, r2, r0, LSL #30
    mcr p15, 0, r2, c7, c6, 2
    add r1, r1, #1
    cmp r1, #256
    bne 2b
    add r0, r0, #1
    cmp r0, #4
    bne 1b
    bx lr

@ Taking in the start address in r0, and the end address in r1 and invalidates
@ the data cache between these two virtual addresses. This might invalidate
@ additional bytes is the address is not aligned with a cache line
@
@ void dcache_invalidate_range(u32 start, u32 end)

.global dcache_invalidate_range
.type dcache_invalidate_range, %function
dcache_invalidate_range:
   
    bic r0, r0, #31          @ Align by cache line
1:  
    mcr p15, 0, r0, c7, c6, 1
    add r0, r0, #32
    cmp r0, r1
    blt 1b
    dsb
    isb
    bx lr

@ Taking in the start address in r0, and the end address in r1 and cleans
@ the data cache between these two virtual addresses. This might clean
@ additional bytes is the address is not aligned with a cache line
@
@ void dcache_clean_range(u32 start, u32 end)

.global dcache_clean_invalidate
.type dcache_clean_invalidate, %function
dcache_clean_invalidate:

    mov r0, #0               @ Way index
1:  
    mov r1, #0               @ Set index
2:  
    mov r2, #0
    orr r2, r2, r1, LSL #5
    orr r2, r2, r0, LSL #30
    mcr p15, 0, r2, c7, c14, 2
    add r1, r1, #1
    cmp r1, #256
    bne 2b
    add r0, r0, #1
    cmp r0, #4
    bne 1b
    bx lr

@ Taking in the start address in r0, and the end address in r1 and cleans and 
@ and invalidates the data cache between these two virtual addresses
.global dcache_clean_invalidate_range
.type dcache_clean_invalidate_range, %function
dcache_clean_invalidate_range:

    bic r0, r0, #31          @ Align the start address by a cache line
1:  
    mcr p15, 0, r0, c7, c14, 1
    add r0, r0, #32
    cmp r0, r1
    blt 1b
    bx lr
