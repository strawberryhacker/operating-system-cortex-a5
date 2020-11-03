/// Copyright (C) strawberryhacker

#ifndef PIXEL_H
#define PIXEL_H

#include <citrus/types.h>

struct pixel {
    u8 glob : 5;
    u8 r;
    u8 b;
    u8 g;
};

struct pixel p1;
u32 tmp;

#endif
