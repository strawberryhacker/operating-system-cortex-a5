/// Copyright (C) strawberryhacker

#ifndef TIMER_H
#define TIMER_H

#include <citrus/types.h>

void timer_init(u32 top, void (*tick_handler)(void));
void timer_start(void);
void timer_stop(void);
void timer_clear_flag(void);
void timer_force_irq(void);
void timer_reset(void);
void timer_get_value(void);


#endif
