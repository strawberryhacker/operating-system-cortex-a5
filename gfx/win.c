// Copyright (C) strawberryhacker

#include <gfx/win.h>
#include <gfx/win_core.h>
#include <citrus/kmalloc.h>
#include <citrus/align.h>
#include <citrus/mem.h>

struct win* create_window(void)
{
    struct win* win = kmalloc(sizeof(struct win));

    // Get the max window size
    win_get_max_size(&win->max_size);

    print("Window size %dx%d\n", win->max_size.x, win->max_size.y);

    // Allocate a buffer 4 byte aligned
    win->buffer = kmalloc(sizeof(struct rgba) * win->max_size.x * win->max_size.y + 4);
    win->buffer = align_up_ptr(win->buffer, 4);

    return win;
}

void win_set_background(struct win* win, struct rgba* color)
{
    u32 col = read_le32(color);

    u32 (*dest)[win->max_size.x] = win->buffer;
    for (u32 i = 0; i < win->size.y; i++) {
        for (u32 j = 0; j < win->size.x; j++) {
            dest[i][j] = col;
        }
    }
}
