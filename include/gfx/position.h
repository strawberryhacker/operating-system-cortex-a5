// Copyright (C) strawberryhacker

#ifndef POSITION_H
#define POSITION_H

#include <citrus/types.h>

struct pos {
    u16 x;
    u16 y;
};

struct size {
    u16 x;
    u16 y;
};

struct region {
    struct pos pos;
    struct size size;
};

#endif
