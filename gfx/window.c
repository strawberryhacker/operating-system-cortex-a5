// Copyright (C) strawberryhacker

#include <gfx/window.h>
#include <gfx/color.h>
#include <gfx/font.h>

#include <citrus/print.h>
#include <citrus/panic.h>
#include <citrus/dma.h>
#include <citrus/mm.h>
#include <citrus/cache.h>
#include <citrus/lcd.h>
#include <citrus/mem.h>
#include <stddef.h>
#include <stdalign.h>

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

    // Screenbuffer used to write pixels to the screen. NOTE: this is double 
    // buffered via the LCD driver and has to be update and switched after
    // each new frame
    struct screenbuffer* screenbuffer;
};

// Private functions
static inline struct update_region* alloc_update_region(void);
static inline void update_region_alloc_reset(void);
static inline void add_window_region(struct window* w, struct list_node* regions);
static void update_regions(struct screen* s);
static void add_window(struct window* w, struct screen* s);
static void print_regions(struct screen* s);
static void screen_init(struct screen* s);
static void build_dma_transfer(struct screen* s);
static inline struct dma_desc3* alloc_dma_desc(void);
static inline struct dma_desc3* dma_desc_get_first(void);
static inline void dma_desc_alloc_reset(void);
static void get_new_screenbuffer(struct screen* s);
void paint_handler(struct screen* screen);

// Both the DMA descriptor view and the window update regions are calculated 
// from scratch each time. We allocate pools for them both
#define UPDATE_REGION_POOL_SIZE 512
static u32 update_region_index = 0;
static struct update_region region_pool[UPDATE_REGION_POOL_SIZE];

// Allocate a pool of view 3 DMA descriptors. This is because we need both 
// microblock source and destination striding which differs from each window
static u32 dma_desc_index = 0;
static struct dma_desc3 alignas(32) dma_desc_pool[UPDATE_REGION_POOL_SIZE];

struct screen screen;

// Allocates a win_reg structure. I will possibly add a real allocator here
// later
static inline struct update_region* alloc_update_region(void)
{
    assert(update_region_index < UPDATE_REGION_POOL_SIZE);
    return &region_pool[update_region_index++];
}

// Resets the win_reg struct allocator, freeing all the blocks
static inline void update_region_alloc_reset(void)
{
    update_region_index = 0;
}

static inline struct dma_desc3* alloc_dma_desc(void)
{
    assert(dma_desc_index < UPDATE_REGION_POOL_SIZE);
    return &dma_desc_pool[dma_desc_index++];
}

static inline struct dma_desc3* dma_desc_get_first(void)
{
    return &dma_desc_pool[0];
}

static inline void dma_desc_alloc_reset(void)
{
    dma_desc_index = 0;
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
        // adjecent regions. This part will add from 0 to to 4 new rectangles

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
// regions which should be drawn. This will effectivly produce a linked list 
// of update regions which willbe used for build a DMA master transfer
static void update_regions(struct screen* s)
{
    // Reset the update region allocator
    update_region_alloc_reset();

    // Iterate the window list in Z-order
    struct list_node* w_node;
    list_iterate_reverse(w_node, &s->window_list) {
        // Get a new window
        struct window* w = list_get_entry(w_node, struct window, w_node);
        
        add_window_region(w, &s->update_list);
    }
}

// Adds a window to the screen
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

// Initialized a screen structure
static void screen_init(struct screen* s)
{
    list_init(&s->update_list);
    list_init(&s->window_list);
}

// All DMA transfer descriptors has the same configuration
static struct dma_mem_mem_info config = {
    .burst = DMA_BURST_16,
    .data = DMA_DATA_U32,
    .dest_am = DMA_AM_STRIDE,
    .src_am = DMA_AM_STRIDE,
    .dest_iface = 0,
    .src_iface = 0,
    .secure = 1,
    .memset = 0
};

// Builds the DMA master transfer from a linked list of update rectangles. This
// functions assumes that all regions are whithin the screen region. This can
// be checked when resizing windows
static void build_dma_transfer(struct screen* s)
{
    // Destination screenbuffer
    u32 (*dest)[s->screenbuffer->w] = 
        (u32 (*)[s->screenbuffer->w])s->screenbuffer->buffer;

    // We track the previous block so that we can link them together
    struct dma_desc3* prev = NULL;
    struct list_node* node;
    list_iterate(node, &s->update_list) {

        // Get the new region which is converted to a DMA transfer
        struct update_region* r = 
            list_get_entry(node, struct update_region, node);
        
        // Allocate a new DMA descriptor
        struct dma_desc3* dma = alloc_dma_desc();

        // Source framebuffer
        struct window* w = r->parent;
        u32 (*src)[w->fb.w] = (u32 (*)[w->fb.w])w->fb.data;

        // Link it with the previous on
        if (prev)
            prev->next = va_to_pa(dma);

        // Number of micro blocks are the height of the rectangle
        dma->block_ctrl  = r->h;
        dma->config      = dma_get_config_mem_mem(&config);
        dma->data_stride = dma_get_data_stride(0, 0);

        // Fetch the data addresses. The window can be at any location (x, y) 
        // but the window buffer is allways at location (0, 0)
        dma->dest_addr   = (u32)va_to_pa((void *)&dest[r->y][r->x]);
        dma->src_addr    = (u32)va_to_pa((void *)&src[r->y - w->y][r->x - w->x]);

        // Compute the source and destination striding
        dma->dest_stride = (s->screenbuffer->w - r->w) * 4;
        dma->src_stride  = (w->fb.w - r->w) * 4;

        // Last?        
        if (node->next == &s->update_list) {
            dma->ublock_ctrl = dma_get_ublock_ctrl(DMA_DESC_TYPE_3, r->w, 0, 1, 1);
        } else {
            dma->ublock_ctrl = dma_get_ublock_ctrl(DMA_DESC_TYPE_3, r->w, 1, 1, 1);
        }

        prev = dma;
    }

    // Last block terminate the list
    prev->next = NULL;

    // Clean the D-cache for the DMA descriptors
    dcache_clean_range((u32)dma_desc_get_first(), (u32)(prev + 1));
}

// Updates the screen object with a new screen buffer
static void get_new_screenbuffer(struct screen* s)
{
    s->screenbuffer = lcd_get_new_screenbuffer(2);
}

u32 buffer_a[480*800];
u32 buffer_b[480*800];
u32 buffer_c[480*800];
u32 buffer_d[480*800];

void fill(void* buff, color_t c, u16 x, u16 y, u16 w, u16 h)
{
    u32 (*b)[800] = buff;
    for (u32 i = 0; i < h; i++) {
        for (u32 j = 0; j < w; j++) {
            b[y + i][x + j] = c;
        }
    }
}

extern const struct simple_font mono9_font;

void done_callback(struct dma_master_req* req)
{
    print("JEYE\n");
    lcd_switch_screenbuffer(2);
}

void window_init(void)
{
    screen_init(&screen);

    struct window a = { .x = 0, .y = 0, .w = 200, .h = 200 };
    struct window b = { .x = 200, .y = 200, .w = 200, .h = 200 };
    struct window c = { .x = 100, .y = 100, .w = 200, .h = 200 };
    struct window d = { .x = 120, .y = 120, .w = 20, .h = 20 };

    a.fb.data = buffer_a;
    a.fb.w = 800;
    b.fb.data = buffer_b;
    b.fb.w = 800;
    c.fb.data = buffer_c;
    c.fb.w = 800;
    d.fb.data = buffer_d;
    d.fb.w = 800;

    a.name[0] = 'A';
    a.name[1] = 0;

    b.name[0] = 'B';
    b.name[1] = 0;

    c.name[0] = 'C';
    c.name[1] = 0;

    d.name[0] = 'D';
    d.name[1] = 0;


    mem_set(a.fb.data, 0x80, 800*480*4);
    mem_set(b.fb.data, 0xFF, 800*480*4);
    mem_set(c.fb.data, 0xAA, 800*480*4);
    mem_set(d.fb.data, 0xFF, 800*480*4);

    fill(a.fb.data, get_rgba(50, 0, 0, 0xFF), 0, 0, 40, 40);
    fill(b.fb.data, get_rgba(50, 0, 0, 0xFF), 0, 0, 200, 40);
    fill(c.fb.data, get_rgba(50, 0, 0, 0xFF), 0, 0, 40, 40);
    fill(d.fb.data, get_rgba(50, 0, 0xFF, 0xFF), 0, 0, 40, 40);

    dcache_clean();

    add_window(&b, &screen);
    add_window(&a, &screen);
    add_window(&c, &screen);
    add_window(&d, &screen);

    update_regions(&screen);

    print("Printing regions\n");
    print_regions(&screen);

    get_new_screenbuffer(&screen);

    build_dma_transfer(&screen);
    print("OK\n");
    
    // Request and DMA channel and perform the transfer
    u8 dma = 0;
    i32 err = get_dma_channel(&dma);
    if (err)
        panic("ok\n");

    struct dma_master_req req = {
        .desc = va_to_pa(dma_desc_get_first()),
        .type = DMA_DESC_TYPE_3,
        .desc_fetch = 1,
        .dest_update = 1,
        .src_update = 1,
        .done = done_callback
    };

    dma_submit_master_req(&req, dma);
}
