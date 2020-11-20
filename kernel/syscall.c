// Copyright (C) strawberryhacker

#include <citrus/syscall.h>
#include <citrus/types.h>
#include <citrus/print.h>
#include <citrus/sched.h>
#include <citrus/thread.h>
#include <citrus/page_alloc.h>
#include <citrus/mm.h>

#define __syscall_def(x)        \
    asm volatile ("svc #"#x""); \
    asm volatile ("bx lr");

#define __syscall(x) __syscall_def(x)

void __svc_attr syscall_thread_sleep(u32 ms)
{
    __syscall(SYSCALL_SLEEP);
}

i32 __svc_attr syscall_create_thread(pid_t* pid, u32 (*func)(void *),
    u32 stack_words, const char* name, void* args, u32 flags)
{
    __syscall(SYSCALL_CREATE_THREAD);
}

u32 __svc_attr syscall_sbrk(u32 bytes)
{
    __syscall(SYSCALL_SBRK);
}

void __svc_attr syscall_kill(struct thread* thread)
{
    __syscall(SYSCALL_KILL);
}

// Called by the SVC vector. The AAPCS stackframe are preserved before this call.
// The LR at the 5th position in the stack frame will contain the return value
// after the SVC vector. The SVC instruction is 4 bytes before the LR causing
// the SVC number to be placed at ofset -4 relative to the SVC LR due the
// little-endian memory ordering
void supervisor_exception(u32* sp)
{
    if (sp[0]) {
        sp += 3;
    } else {
        sp += 2;
    }
    
    // The SP is pointing to the r0 in the standard interrupt stack frame
    u8  svc_num = ((u8 *)*(sp + 5))[-4];

    u32 svc0 = sp[0];
    u32 svc1 = sp[1];
    u32 svc2 = sp[2];
    u32 svc3 = sp[3];

    switch(svc_num) {
        case SYSCALL_SLEEP : {
            sched_thread_sleep(svc0);
            break;
        }
        case SYSCALL_CREATE_THREAD : {
            sp[0] = (u32)create_thread((i32 (*)(void *))svc0, svc1,
                (const char *)svc2, (void *)svc3, sp[7]);
            break;
        }
        case SYSCALL_SBRK : {
            sp[0] = (u32)set_break(sp[0]);
            break;
        }
        case SYSCALL_KILL : {
            //kill_thread((struct thread *)svc0);
        }
    }
}
