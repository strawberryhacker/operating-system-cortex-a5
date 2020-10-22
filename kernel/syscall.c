/// Copyright (C) strawberryhacker

#include <cinnamon/types.h>
#include <cinnamon/print.h>
#include <cinnamon/sched.h>
#include <cinnamon/thread.h>
#include <cinnamon/page_alloc.h>
#include <cinnamon/mm.h>

/// Called by the SVC vector. The AAPCS stackframe are preserved before this call.
/// The LR at the 5th position in the stack frame will contain the return value
/// after the SVC vector. The SVC instruction is 4 bytes before the LR causing
/// the SVC number to be placed at ofset -4 relative to the SVC LR due the
/// little-endian memory ordering
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
        case 0 : {
            sp[0] = (u32)create_thread((u32 (*)(void *))svc0, svc1,
                (const char *)svc2, (void *)svc3, sp[7]);
            break;
        }
        case 1 : {
            sp[0] = (u32)set_break(sp[0]);
            break;
        }
        case 2: {
            alloc_page();
            break;
        }
        case 8 : {
            sched_thread_sleep(svc0);
            break;
        }
        case 9 : {
            u32 tmp;
            asm volatile ("mrs %0, spsr" : "=r" (tmp));
            sp[0] = tmp;
            break;
        }
    }
}
