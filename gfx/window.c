// Copyright (C) strawberryhacker

#include <gfx/window.h>
#include <citrus/print.h>
#include <citrus/panic.h>
#include <stddef.h>

// The windows are stored in Z-order in a linked list. Due to the limited memory
// resources on this system, the windows are processed to avoid overlapping. 
// The collectin of windows is broken down into rectangles, each having a
// pointer to the parent structure. On each update these rectangles will be
// used to build a DMA linked list master transfers consisting of all the 
// rectangles. This way the entire update does not use CPU and does only update
// one region once. 
//
// This also allows for starting the transfer while the event are being 
// processed. This requires the DMA to be set up twice but can save some time. 
// This structure are used to mark different regions of a parent window for
// update.
struct update_region {
    u16 x, y, w, h;

    // List for grouping all the update regions together
    struct list_node node;

    // Pointer to parent window
    struct window* parent;
};

struct screen {
    // List with all the window update regions
    struct list_node update_list;

    // Number of windows in the system
    u32 window_cnt;

    // List of all the windows in the system
    struct list_node window_list;
};

// Both the DMA descriptor view and the window update regions are calculated 
// from scratch each time. We allocate pools for them both
#define UPDATE_REGION_POOL_SIZE 512
static u32 update_region_index = 0;
struct update_region region_pool[UPDATE_REGION_POOL_SIZE];

struct screen screen;

// Allocates a win_reg structure. I will possibly add a real allocator here
// later
static inline struct update_region* alloc_update_region(void)
{
    assert(update_region_index < UPDATE_REGION_POOL_SIZE);
    return &region_pool[update_region_index++];
}

// Resets the win_reg struct allocator, freeing all the blocks
static inline void win_reg_alloator_reset(void)
{
    update_region_index = 0;
}

// This functions will add a window to the update list. Basically, the window
// Will be placed on top as a complete rectangle. The layers / rectangels
// underneath it will be split into smaller non-overlapping rectangles
static inline void add_window_region(struct window* w, struct list_node* regions)
{
    // For each new window, no more than one window will be updated. Therefore
    // we use only one list for add
    struct list_node add_list;
    list_init(&add_list);

    // Point to the node to be deleted in non-zero
    struct list_node* del_node = NULL;

    struct list_node* node;
    list_iterate(node, regions) {

        // Delete last node if non-zero
        if (del_node)
            list_delete_node(del_node);

        // Grab a new non overlapping region
        struct update_region* r = 
            list_get_entry(node, struct update_region, node);
        
        // +---------------------------+
        // |                           |
        // +---+--------------+--------+
        // |   |  new window  |        |
        // |   |              |        |
        // +---+--------------+--------+
        // |               wx2,wy2     |
        // |                           |
        // +---------------------------+
        //                          rx2,ry2
        // The window is divided into four smaller rectangles to uptimize the
        // DMA transfer. It is really nothing to get from trying to combine 
        // adjecent regions...

        // End locations
        u16 wx2 = w->x + w->w;
        u16 wy2 = w->y + w->h;
        u16 rx2 = r->x + r->w;
        u16 ry2 = r->y + r->h;

        // Assume no overlap
        del_node = NULL;

        // This will determine if we have to delete the existing node
        if (w->x > rx2 || w->y > ry2 || wx2 < r->x || wy2 < r->y)
            continue;

        u8 top_active = 0;
        u8 bottom_active = 0;

        // Check top region
        if (w->y > r->y && w->y < ry2) {
            struct update_region* new = alloc_update_region();
            list_add_first(&new->node, &add_list);

            new->parent = r->parent;
            new->x = r->x;
            new->y = r->y;
            new->w = r->w;
            new->h = w->y - r->y;
            del_node = node;

            top_active = 1;
        }

        // Check bottom region
        if (wy2 > r->y && wy2 < ry2) {
            struct update_region* new = alloc_update_region();
            list_add_first(&new->node, &add_list);
            
            new->parent = r->parent;
            new->x = r->x;
            new->y = wy2;
            new->w = r->w;
            new->h = ry2 - wy2;
            del_node = node;

            bottom_active = 1;
        }

        // Check left region
        if (w->x > r->x && w->x < rx2) {
            struct update_region* new = alloc_update_region();
            list_add_first(&new->node, &add_list);
            
            new->parent = r->parent;
            new->x = r->x;
            new->y = (top_active) ? w->y : r->y;
            new->w = w->x - r->x;
            new->h = (bottom_active) ? wy2 - new->y : ry2 - new->y;
            del_node = node;
        }

        // Check right region
        if (wx2 < rx2 && wx2 > r->x) {
            struct update_region* new = alloc_update_region();
            list_add_first(&new->node, &add_list);
            
            new->parent = r->parent;
            new->x = wx2;
            new->y = (top_active) ? w->y : r->y;
            new->w = rx2 - wx2;
            new->h = (bottom_active) ? wy2 - new->y : ry2 - new->y;
            del_node = node;
        }
    }

    if (del_node)
            list_delete_node(del_node);

    // The new region should be added on top anyway
    struct update_region* new = alloc_update_region();
    list_add_first(&new->node, &add_list);

    new->x = w->x;
    new->y = w->y;
    new->h = w->h;
    new->w = w->w;
    new->parent = w;

    // Merge lists together
    list_merge(&add_list, regions);
}

// Goes though all the active windows and updates the region list with all the 
// regions which should be drawn
static void update_regions(struct screen* s)
{
    // Iterate the window list in Z-order
    struct list_node* w_node;
    list_iterate_reverse(w_node, &s->window_list) {
        // Get a new window
        struct window* w = list_get_entry(w_node, struct window, w_node);
        
        add_window_region(w, &s->update_list);
    }
}

static void add_window(struct window* w, struct screen* s)
{
    list_add_first(&w->w_node, &s->window_list);
}

static void print_regions(struct screen* s)
{
    struct list_node* node;
    list_iterate(node, &s->update_list) {
        struct update_region* r = 
            list_get_entry(node, struct update_region, node);
        
        print("Region > X %d Y %d W %d H %d\n", r->x, r->y, r->w, r->h);
    }
}

void screen_init(struct screen* screen)
{
    list_init(&screen->update_list);
    list_init(&screen->window_list);
}

void window_init(void)
{
    screen_init(&screen);

    struct window a = { .x = 2, .y = 2, .w = 4, .h = 3 };
    struct window b = { .x = 0, .y = 3, .w = 4, .h = 3 };
    struct window c = { .x = 3, .y = 0, .w = 4, .h = 4 };
    struct window d = { .x = 4, .y = 2, .w = 1, .h = 1 };

    add_window(&a, &screen);
    add_window(&b, &screen);
    add_window(&c, &screen);
    add_window(&d, &screen);

    update_regions(&screen);

    print("Printing regions\n");
    print_regions(&screen);
}
