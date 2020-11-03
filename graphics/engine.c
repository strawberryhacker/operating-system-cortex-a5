/// Copyright (C) strawberryhacker

#include <graphics/engine.h>

#include <citrus/print.h>
#include <citrus/kmalloc.h>
#include <citrus/list.h>
#include <citrus/syscall.h>
#include <citrus/panic.h>
#include <app/led_strip.h>

void engine_draw(struct engine* e)
{
    led_strip_update(e->pixels, e->pixel_cnt);
}

void engine_init(struct engine* e)
{
    e->pixel_cnt = 16;
    e->pixels = kmalloc(16 * sizeof(struct pixel));
    e->draw = &engine_draw;
}
