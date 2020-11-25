// Copyright (C) strawberryhacker

#ifndef WIN_H
#define WIN_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <gfx/position.h>
#include <citrus/lcd.h>

// Main window structure
struct win {
    void* buffer;
    struct size buffer_size;

    // Set to one if the window should be rendered to the screen
    u8 active;

    struct size max_size;

    struct size size;
    struct pos pos;

    struct list_node node;
};

struct win* create_window(void);

void win_set_background(struct win* win, struct rgba* color);

#endif
