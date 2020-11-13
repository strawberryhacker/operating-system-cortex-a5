/// Copyright (C) strawberryhacker

#ifndef GFX_H
#define GFX_H

#include <citrus/types.h>
#include <citrus/lcd.h>

struct font_glyph {
    u16 bitmap_offset;
    u8 width;
    u8 height;
    u8 x_advance;
    i8 x_offset;
    i8 y_offset;
};

struct font {
    u8* bitmap;
    struct font_glyph* glyph;
    u16 first;
    u16 last;
    u8 y_advance;
};

extern struct font mono_12pt;

void draw_char(struct rgb* fb, u16 width, u16 x, u16 y, char c);

#endif
