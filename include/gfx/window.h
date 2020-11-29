// Copyright (C) strawberryhacker

#ifndef WINDOW_H
#define WINDOW_H

#include <citrus/types.h>
#include <citrus/list.h>

// Structure for a display window. This will contain just the necessary part
// to get the windows to be displayed and updated.
struct window {

    // Position which maps the window onto the screen
    u16 x, y, h, w;

    // Window list in z-order. Windows which appear longest away on the screen
    // is placed last in the window list
    struct list_node w_node;
};

void window_init(void);

#endif
