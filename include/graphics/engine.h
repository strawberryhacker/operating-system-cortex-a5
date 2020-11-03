/// Copyright (C) strawberryhacker

#ifndef ENGINE_H
#define ENGINE_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <graphics/pixel.h>

struct engine {
    
    // Functions for updating display
    void (*draw)(struct engine*);

    // Private pixel data
    struct pixel* pixels;
    u32 pixel_cnt;
};

void engine_init(struct engine* e);

#endif
