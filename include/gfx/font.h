// Copyright (C) strawberryhacker

#ifndef FONT_H
#define FONT_H

#include <citrus/types.h>
#include <citrus/lcd.h>

struct glyph {
    u16 off;
    u8 w;
    u8 h;
    u8 x_advance;
    i8 x_off;
    i8 y_off;
};

struct simple_font {
    u8* bitmap;
    struct glyph* glyph;
    u16 first;
    u16 last;
    u8 y_advance;
};

void font_test(void);

void print_screen(const char* text, u16 x, u16 y, const struct simple_font* font, struct rgba (*fb)[800]);

#endif
