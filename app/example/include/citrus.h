/// Copyright (C) strawberryhacker

#ifndef CITRUS_H
#define CITRUS_H

#include <types.h>

#define SVC_ATTR __attribute__((naked)) __attribute__((noinline))

#define __SVC(x)        \
    asm volatile (      \
        "svc #"#x"\t\n" \
        "bx lr\n\t");

static void SVC_ATTR syscall_thread_sleep(u32 us)
{
    __SVC(8);
}

static u32 SVC_ATTR syscall_create_thread(void (*func)(void *),
    u32 stack_size, const char* name, void* args, u32 flags)
{
    __SVC(0);
}


#endif
