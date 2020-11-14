// Copyright (C) strawberryhacker

#include <citrus/clock.h>
#include <citrus/regmap.h>

// Enables a peripheral clock by its PID
void clk_pck_enable(u32 pid)
{
    if ((pid >= 2) && (pid < 32)) {
        PMC->PCER0 = (1 << pid);
    } else if ((pid >= 32) && (pid < 64)) {
        pid -= 32;
        PMC->PCER1 = (1 << pid);
    } else {
        PMC->PCR = pid & 0x7F;
        u32 reg = PMC->PCR;
        PMC->PCR = reg | (1 << 12) | (1 << 28);
    }
}

// Disables a peripheral clock by its PID
void clk_pck_disable(u32 pid)
{
    if ((pid >= 2) && (pid < 32)) {
        PMC->PCDR0 = (1 << pid);
    } else if ((pid >= 32) && (pid < 64)) {
        pid -= 32;
        PMC->PCDR1 = (1 << pid);
    } else {
        PMC->PCR = pid & 0x7F;
        u32 reg = PMC->PCR;
        reg &= ~(1 << 28);
        PMC->PCR = reg | (1 << 12);
    }
}

void clk_gck_enable(u8 pid, enum gck_src src, u8 div)
{
    /* Write mode */
    PMC->PCR = pid & 0x7F;
    u32 en = (PMC->PCR & (1 << 28)) ? 1 : 0;
    PMC->PCR = (en << 28) | (src << 8) | (1 <<12) | (div << 20) | (1 << 29) |
        (pid & 0x7F);
}
