// Copyright (C) strawberryhacker

#ifndef WINDOW_CORE_H
#define WINDOW_CORE_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <citrus/lcd.h>
#include <gfx/position.h>
#include <gfx/win.h>

// Window manager
struct win_core {

    // Information of the current physical framebuffer
    struct fb_info* fb;

    // Linked list of all the windows in z-order
    struct list_node win_list;
};

void win_core_init(void);

void win_core_paint(void);

void attatch_window(struct win* win);

void win_focus(struct win* win);

void win_get_max_size(struct size* size);

#endif
