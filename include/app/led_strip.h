/// Copyright (C) strawberryhacker

#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <citrus/types.h>
#include <graphics/pixel.h>

void led_strip_init(void);

u32 led_strip_thread(void* args);

void led_strip_update(struct pixel* pixels, u32 cnt);

#endif
