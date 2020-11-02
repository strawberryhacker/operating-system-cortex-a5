/// Copyright (C) strawberryhacker

#ifndef PIT_H
#define PIT_H

#include <citrus/types.h>

void pit_init(u8 irq_en, u32 period);

void pit_enable(void);

void pit_disable(void);

void pit_clear_flag(void);

#endif
