// Copyright (C) strawberryhacker

#include <gfx/engine.h>

#include <citrus/print.h>
#include <citrus/kmalloc.h>
#include <citrus/list.h>
#include <citrus/syscall.h>
#include <citrus/panic.h>
#include <app/led_strip.h>
#include <gfx/pixel.h>

void engine_draw(struct engine* e)
{
    led_strip_update(e->pixels, e->pixel_cnt);
}

void engine_init(struct engine* e)
{
    e->pixel_cnt = 16;
    e->pixels = kmalloc(16 * sizeof(struct pixel));
    e->draw = engine_draw;

    // Initialize the pixels
    struct pixel* p = e->pixels;
    for (u32 i = 0; i < e->pixel_cnt; i++) {
        *p = (struct pixel){ .blue = 0, .red = 0, .green = 0, .glob = 15 };
        p++;
    }

    e->draw(e);
}
