/// Copyright (C) strawberryhacker

#ifndef PID_H
#define PID_H

#include <citrus/types.h>

void pid_init(void);
i8 alloc_pid(u32* pid);
void free_pid(u32 pid);

#endif
