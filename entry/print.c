// Copyright (C) strawberryhacker 

#include <citrus/print.h>
#include <citrus/sprint.h>
#include <citrus/regmap.h>

static char buffer[1024];

void print(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buffer, data, args);
    va_end(args);

    const u8* start = (const u8 *)buffer;
    while (cnt--) {
        while (!(UART1->SR & (1 << 1)));
        UART1->THR = *start;
        start++;
    }
}
