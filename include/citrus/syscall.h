/// Copyright (C) strawberryhacker

#ifndef SYSCALL_H
#define SYSCALL_H

#include <citrus/types.h>
#include <citrus/print.h>

#define SYSCALL_SLEEP         0
#define SYSCALL_CREATE_THREAD 1
#define SYSCALL_SBRK          2

#define SVC_ATTR __attribute__((naked)) __attribute__((noinline))

#define SYSCALL_NX(x)      \
    asm volatile (         \
        "svc #"#x"  \t\n"  \
        "bx lr      \n\t"  \
    );

#define SYSCALL(x) SYSCALL_NX(x)

static void SVC_ATTR syscall_thread_sleep(u32 us)
{
    SYSCALL(SYSCALL_SLEEP);
}

static struct thread* SVC_ATTR syscall_create_thread(u32 (*func)(void *),
    u32 stack_size, const char* name, void* args, u32 flags)
{
    SYSCALL(SYSCALL_CREATE_THREAD);
}

static u32 SVC_ATTR syscall_sbrk(u32 bytes)
{
    SYSCALL(SYSCALL_SBRK);
}

#endif
