/// Copyright (C) strawberryhacker

#ifndef CLOCK_H
#define CLOCK_H

#include <citrus/types.h>

enum gck_src {
    GCK_SRC_SLOW_CLK,
    GCK_SRC_MAIN_CLK,
    GCK_SRC_PLLA_CLK,
    GCK_SRC_UPLL_CLK,
    GCK_SRC_MCK_CLK,
    GCK_SRC_AUDIO_CLK,
};

void clk_pck_enable(u32 pid);
void clk_pck_disable(u32 pid);

void clk_gck_enable(u8 pid, enum gck_src src, u8 div);

#endif
