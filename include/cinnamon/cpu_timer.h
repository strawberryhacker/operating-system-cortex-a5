/// Copyright (C) strawberryhacker

#ifndef CPU_TIMER_H
#define CPU_TIMER_H

#include <cinnamon/types.h>

/// Gives a overflow every millisecond
void cpu_timer_init(void);
void cpu_timer_reset(void);
void cpu_timer_clear_flags(void);
u32 cpu_timer_get_value(void);
void cpu_timer_start(void);

#endif
