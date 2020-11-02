/// Copyright (C) strawberryhacker

#ifndef PID_H
#define PID_H

#include <citrus/types.h>

#define MAX_PIDS 1024

void pid_init(void);

u32 alloc_pid(void);

void free_pid(u32 pid);

#endif
