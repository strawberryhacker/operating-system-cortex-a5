/// Copyright (C) strawberryhacker

#ifndef LCD_H
#define LCD_H

#include <citrus/types.h>

#define SCREEN_X 800
#define SCREEN_Y 480
#define LAYERS 3

struct lcd_info {
    u16 w;
    u16 h;
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

// Framebuffer info
struct screenbuffer {
    void* buffer;
    u16 h;
    u16 w;
    u8 bpp;
};

struct rgb {
    u8 b;
    u8 g;
    u8 r;
};

struct rgba {
    u8 a;
    u8 b;
    u8 g;
    u8 r;
};

void lcd_layers_alloc(void);

void lcd_init(void);

void lcd_set_brightness(u8 brightness);

void lcd_on(struct lcd_info* info);

void lcd_off(void);

void lcd_switch_screenbuffer(u8 layer);

struct screenbuffer* lcd_get_new_screenbuffer(u8 layer);

void lcd_get_size(u16* x, u16* y, u8 layer);

#endif
