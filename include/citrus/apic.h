/// Copyright (C) strawberryhacker 

#ifndef APIC_H
#define APIC_H

#include <citrus/types.h>
#include <citrus/regmap.h>


void apic_protect(struct apic_reg* apic);
void apic_init(void);
void apic_add_handler(u32 irq, void (*handler)(void));
void apic_remove_handler(u32 irq);
void apic_enable(u32 irq);
void apic_disable(u32 irq);
void apic_force(u32 irq);
void apic_clear(u32 irq);
void apic_set_priority(u32 irq, u32 pri);
u32 apic_get_max_priority(u32 irq);
u32 apic_get_min_priority(u32 irq);


#endif
