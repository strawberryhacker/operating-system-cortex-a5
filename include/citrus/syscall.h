/// Copyright (C) strawberryhacker

#ifndef SYSCALL_H
#define SYSCALL_H

#include <citrus/types.h>
#include <citrus/print.h>
#include <citrus/thread.h>
#include <citrus/pid.h>

#define SYSCALL_SLEEP         0
#define SYSCALL_CREATE_THREAD 1
#define SYSCALL_SBRK          2
#define SYSCALL_KILL          3

#define __svc_attr __attribute__((naked)) __attribute__((noinline))

void __svc_attr syscall_thread_sleep(u32 ms);

i32 __svc_attr syscall_create_thread(pid_t* pid, u32 (*func)(void *),
    u32 stack_words, const char* name, void* args, u32 flags);

u32 __svc_attr syscall_sbrk(u32 bytes);

void __svc_attr syscall_kill(struct thread* thread);

#endif
