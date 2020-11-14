// Copyright (C) strawberryhacker 

#include <citrus/pit.h>
#include <citrus/regmap.h>

void pit_init(u8 irq_en, u32 period)
{
    u32 reg = ((irq_en & 1) << 25) | (period & 0xFFFFF);
    PIT->MR = reg;
}

void pit_enable(void)
{
    PIT->MR |= (1 << 24);
}

void pit_disable(void)
{
    PIT->MR &= ~(1 << 24);
}

void pit_clear_flag(void)
{
    (void)PIT->PIVR;
}
