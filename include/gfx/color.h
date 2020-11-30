// Copyright (C) strawberryhacker

#ifndef COLOR_H
#define COLOR_H

#include <citrus/types.h>

// This system uses 32-bit alpha compositing color
typedef u32 color_t;

// Colors are stored in little endian format. The memory color format is, from
// lowest to highest memory address; A, B, G, R. This assumes that the CPU will
// write the 32-bit color to memory
static inline color_t get_rgba(u8 r, u8 g, u8 b, u8 a)
{
    return (r << 24) | (g << 16) | (b << 8) | a;
}

#endif
