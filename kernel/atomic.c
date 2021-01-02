/// Copyright (C) strawberryhacker

#include <citrus/atomic.h>

void __print_cpsr(void)
{
    u32 cpsr;
    asm volatile ("mrs %0, cpsr" : "=r" (cpsr));
    print("CPSR => %8b\n", cpsr);
}

/// Enters atomic mode and saves the interrup flags
u32 __atomic_enter(void)
{
    u32 flags;
    asm volatile (
        "mrs %0, cpsr      \n\t"
        "cpsid ifa          \n\t"
        "and %0, %0, #0xc0 \n\t"
    : "=r" (flags));

    return flags;
}

/// Leaves atomic mode and restores the interrupt flags
void __atomic_leave(u32 flags)
{
    u32 reg = 0;
    asm volatile (
        "mrs %0, cpsr      \n\t"
        "bic %0, %0, #0xc0 \n\t"
        "orr %1, %0, %1    \n\t"
        "msr cpsr, %1      \n\t"
    : "=r" (reg) : "r" (flags));
}
