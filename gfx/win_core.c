// Copyright (C) strawberryhacker

#include <gfx/win_core.h>
#include <gfx/win.h>
#include <citrus/atomic.h>
#include <citrus/print.h>
#include <citrus/cache.h>

static struct win_core win_core;

static void init_win_core_struct(struct win_core* core)
{
    list_init(&core->win_list);

    // Get the current framebuffer for layer 1
    win_core.fb = lcd_get_new_framebuffer(1);

    print("Frame buffer pointer %p\n", win_core.fb);
}

void win_core_init(void)
{
    print("Starting windows core\n");

    // Initilaize the structure
    init_win_core_struct(&win_core);
}

// Paint handler which will paint all the windows
void win_core_paint(void)
{
    // Clear the buffer
    print("clearing buffer\n");
    

    u32 size = win_core.fb->width * win_core.fb->height;
    u32* ptr = win_core.fb->buffer;
    for (u32 i = 0; i < size; i++) {
        *ptr++ = 0x00;
    }

    u32 (*dest)[win_core.fb->width] = win_core.fb->buffer;
    struct list_node* it;
    list_iterate(it, &win_core.win_list) {
        struct win* win = list_get_entry(it, struct win, node);

        print("Updating window %dx%d\n", win->size.x, win->size.y);
        u32 (*src)[win_core.fb->width] = win->buffer;
        
        for (u32 j = 0; j < win->size.y; j++) {
            for (u32 i = 0; i < win->size.x; i++) {
                dest[win->pos.y + j][win->pos.x + i] = src[j][i];
            }
        }
    }
    dcache_clean();

    lcd_switch_framebuffer(1);
    win_core.fb = lcd_get_new_framebuffer(1);

    print("Paint done\n");
}

// Attatched a new window to the window manager. The new window will be placed
// in front of the queue
void attatch_window(struct win* win)
{
    u32 atomic = __atomic_enter();
    list_add_first(&win->node, &win_core.win_list);
    __atomic_leave(atomic);
}

// Gives focus to the specified window. The window must be attatched for this 
// be called
void win_focus(struct win* win)
{
    // Place the window at the back
    u32 atomic = __atomic_enter();
    list_delete_node(&win->node);
    list_add_first(&win->node, &win_core.win_list);
    __atomic_leave(atomic);
}

void win_get_max_size(struct size* size)
{
    size->x = win_core.fb->width;
    size->y = win_core.fb->height;
}
