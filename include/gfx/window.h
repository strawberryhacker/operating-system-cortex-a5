// Copyright (C) strawberryhacker

#ifndef WINDOW_H
#define WINDOW_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <gfx/event.h>

#define WINDOW_MAX_NAME 64

struct framebuffer {
    void* data;
    u16 w, h;
};

// Structure for a display window. This will contain just the necessary part
// to get the windows to be displayed and updated.
struct window {

    // Position which maps the window onto the screen
    u16 x, y, h, w;
    u16 new_x, new_y, new_h, new_w;

    // Max size used to limit the amount of data needed in the framebuffer
    u16 max_w, max_h;

    // Window list node in z-order. Windows which appear longest away on the 
    // screen is placed last in the window list
    struct list_node w_node;

    // Name of the window
    char name[WINDOW_MAX_NAME];

    // Framebuffer data which will source the DMA transfers
    struct framebuffer fb;

    // Prive data for the GUI / widget / tiling window manager system
    void* private;

    // Send when the window manager wants to update parts of the application
    void (*paint_region)(struct window* w);

    // Event handlers
    void (*focus)(struct event* e);
};

void window_init(void);

#endif
