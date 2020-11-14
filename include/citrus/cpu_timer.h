/// Copyright (C) strawberryhacker

#ifndef CPU_TIMER_H
#define CPU_TIMER_H

#include <citrus/types.h>

// Information to the scheduler - how many us in a slice
#define SCHED_SLICE 1000

// Gives a overflow every millisecond
void cpu_timer_init(void);
void cpu_timer_reset(void);
void cpu_timer_clear_flags(void);
u32 cpu_timer_get_value(void);
u32 cpu_timer_get_value_us(void);
void cpu_timer_start(void);

#endif
