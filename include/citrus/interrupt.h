/// Copyright (C) strawberryhacker

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <citrus/types.h>

static inline void irq_enable(void)
{
    asm volatile ("cpsie i");
}

static inline void irq_disable(void)
{
    asm volatile ("cpsid i");
}

static inline void fiq_enable(void)
{
    asm volatile ("cpsie f");
}

static inline void fiq_disable(void)
{
    asm volatile ("cpsid f");
}

static inline void async_abort_enable(void)
{
    asm volatile ("cpsie a");
}
static inline void async_abort_disable(void)
{
    asm volatile ("cpsid a");
}

#endif
