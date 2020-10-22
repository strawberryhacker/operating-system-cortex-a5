/// Copyright (C) strawberryhacker

#ifndef SYSCALL_H
#define SYSCALL_H

#include <cinnamon/types.h>
#include <cinnamon/print.h>

#define SVC_ATTR __attribute__((naked)) __attribute__((noinline))

#define __SVC(x)       \
    asm volatile (     \
        "svc #"#x"\t\n"\
        "bx lr\n\t");

static void SVC_ATTR syscall_thread_sleep(u32 us)
{
    __SVC(8);
}

static u32 SVC_ATTR syscall_get_cpsr(void)
{
    __SVC(9);
}

static struct thread* SVC_ATTR syscall_create_thread(u32 (*func)(void *),
    u32 stack_size, const char* name, void* args, u32 flags)
{
    __SVC(0);
}

u32* SVC_ATTR sbrk(u32 bytes)
{
    __SVC(1);
}

void SVC_ATTR syscall_alloc(void)
{
    __SVC(2);
}

#endif
