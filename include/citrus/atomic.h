/// Copyright (C) strawberryhacker

#ifndef ATOMIC_H
#define ATOMIC_H

#include <citrus/types.h>
#include <citrus/print.h>

void __print_cpsr(void);
u32 __atomic_enter(void);
void __atomic_leave(u32 flags);

#endif
