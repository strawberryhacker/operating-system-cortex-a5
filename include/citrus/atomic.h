/// Copyright (C) strawberryhacker

#ifndef ATOMIC_H
#define ATOMIC_H

#include <citrus/types.h>
#include <citrus/print.h>

static void __print_cpsr(void)
{
    u32 cpsr;
    asm volatile ("mrs %0, cpsr" : "=r" (cpsr));
    print("CPSR => %8b\n", cpsr);
}

/// Enters atomic mode and saves the interrup flags
static u32 __atomic_enter(void)
{
    u32 flags;
    asm volatile (
        "mrs %0, cpsr      \n\t"
        "cpsid if          \n\t"
        "and %0, %0, #0xc0 \n\t"
    : "=r" (flags));

    return flags;
}

/// Leaves atomic mode and restores the interrupt flags
static void __atomic_leave(u32 flags)
{
    u32 reg = 0;
    asm (
        "mrs %0, cpsr      \n\t"
        "bic %0, %0, #0xc0 \n\t"
        "orr %1, %0, %1    \n\t"
        "msr cpsr_c, %1    \n\t"
    : "=r" (reg) : "r" (flags));
}

#endif
