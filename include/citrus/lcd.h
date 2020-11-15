/// Copyright (C) strawberryhacker

#ifndef LCD_H
#define LCD_H

#include <citrus/types.h>

struct lcd_info {
    u16 width;
    u16 height;
    u8 framerate;

    // Porch in number of lines
    u8 v_front_porch;
    u8 v_back_porch;
    u8 v_pulse_width;

    // Porch in number of cycles
    u8 h_front_porch;
    u8 h_back_porch;
    u8 h_pulse_width;
};

struct lcd_dma_desc {
    u32 addr;
    u32 ctrl;
    u32 next;
    u32 padding;
};

u32 get_color(u8 r, u8 g, u8 b, u8 a);

void set_color(u32* buffer, u16 x, u16 y, u16 w, u16 h, u32 color);

void lcd_init(void);

void lcd_set_brightness(u8 brightness);

void lcd_on(struct lcd_info* info);

void lcd_off(void);

u32* lcd_get_framebuffer(u8 layer);

void test(void);

#endif
