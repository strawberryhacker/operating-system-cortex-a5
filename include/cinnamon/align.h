/// Copyright (C) strawberryhacker 

#ifndef ALIGN_H
#define ALIGN_H

#include <cinnamon/types.h>

static inline void* align_up(void* ptr, u32 align_bytes)
{
    u32 _ptr = (u32)ptr;
    if (_ptr & (align_bytes - 1)) {
        _ptr += align_bytes;
        _ptr &= ~(align_bytes - 1);
    }
    return (void *)_ptr;
}

static inline void* align_down(void* ptr, u32 align_bytes)
{
    u32 _ptr = (u32)ptr;
    _ptr &= ~(align_bytes - 1);
    return (void *)_ptr;
}

static inline u32 align_up_val(u32 val, u32 align_bytes)
{
    if (val & (align_bytes - 1)) {
        val += align_bytes;
        val &= ~(align_bytes - 1);
    }
    return val;
}

static inline u32 align_down_val(u32 val, u32 align_bytes)
{
    val &= ~(align_bytes - 1);
    return val;
}

static inline u32 round_up_power_two(u32 number)
{
    number--;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    return ++number;
}

static inline u32 round_down_power_two(u32 number)
{
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    return number - (number >> 1);
}

#endif
