/// Copyright (C) strawberryhacker

#include <cinnamon/clock.h>
#include <regmap.h>

/// Enables a peripheral clock by its PID
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

/// Disables a peripheral clock by its PID
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
