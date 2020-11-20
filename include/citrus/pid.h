/// Copyright (C) strawberryhacker

#ifndef PID_H
#define PID_H

#include <citrus/types.h>

typedef u32 pid_t;

void pid_init(void);
i32 alloc_pid(pid_t* pid);
void free_pid(pid_t pid);

#endif
