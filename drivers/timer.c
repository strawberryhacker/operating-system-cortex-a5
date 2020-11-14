// Copyright (C) strawberryhacker

#include <citrus/timer.h>
#include <citrus/apic.h>
#include <citrus/clock.h>
#include <citrus/regmap.h>

void timer_init(u32 top, void (*tick_handler)(void))
{
    clk_pck_enable(35);
    
    // Configure the timer mode; waveform mode, reset on RC compare, running
    // from PCK @ 83 MHz divided by 128 - yielding 648 kHz
    TIMER0->channel[0].CMR = (1 << 7) | (1 << 15) | 3;

    TIMER0->channel[0].RC = top;

    // Enable RC compare interrupt 
    TIMER0->channel[0].IER = (1 << 4);

    apic_add_handler(35, tick_handler);
    apic_set_priority(35, 2);
    apic_enable(35);
}

void timer_start(void)
{
    TIMER0->channel[0].CCR = (1 << 2) | 1;
}

void timer_stop(void)
{
    TIMER0->channel[0].CCR = 0b10;
}

void timer_clear_flag(void)
{
    (void)TIMER0->channel[0].SR;
}

void timer_force_irq(void)
{

}

void timer_reset(void)
{
    TIMER0->channel[0].CCR = (1 << 2) | 1;
}

void timer_get_value(void)
{

}
