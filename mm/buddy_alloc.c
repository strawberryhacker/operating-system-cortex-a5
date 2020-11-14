// Copyright (C) strawberryhacker 

#include <citrus/buddy_alloc.h>
#include <citrus/print.h>
#include <citrus/mem.h>
#include <citrus/kmalloc.h>
#include <citrus/align.h>
#include <citrus/panic.h>
#include <citrus/atomic.h>

static inline u32 get_order_map_words(u32 order, u32 max_orders)
{
    u32 tmp = (1 << (max_orders - order - 1));
    tmp >>= 5;
    if (tmp == 0) {
        tmp++;
    }
    return tmp;
}

// This returns the number of words for the entire buddy bitmap, excluded the
// order zero
static inline u32 get_map_size(u32 max_orders) {
    u32 num_blocks = (1 << (max_orders - 2));
    return ((num_blocks << 1) >> 5) + 4;
}

// Gets the value of a bit counted from a pointer base. Returns 0 if the bit is 
// zero
static inline u32 get_bit(u32 bit, void* ptr)
{
    u32* src = (u32 *)ptr;

    return src[bit / 32] & (1 << (bit % 32)); 
}

static inline void toggle_bit(u32 bit, void* ptr)
{
    u32* src = (u32 *)ptr;

    src[bit / 32] ^= (1 << (bit % 32)); 
}

// Gets the total number of bytes free
static u32 buddy_get_free(struct mm_zone* zone)
{
    u32 total = zone->page_cnt * 4096;
    struct buddy_struct* buddy = (struct buddy_struct *)zone->alloc;
    return total - buddy->used;
}

// Gets the total number of bytes used
static u32 buddy_get_used(struct mm_zone* zone)
{
    struct buddy_struct* buddy = (struct buddy_struct *)zone->alloc;
    return buddy->used;
}

// Gets the total number of bytes in total
static u32 buddy_get_total(struct mm_zone* zone)
{
    return zone->page_cnt * 4096;
}

// Initailzie the zone structure used for the buddy allocator
static void buddy_init_zone(struct mm_zone* zone)
{
    zone->get_used = buddy_get_used;
    zone->get_total = buddy_get_total;
    zone->get_free = buddy_get_free;
}

// The zone should have a pointer to the buddy structure. The buddy structure
// need a bitmap and a order list in order to work. This should ideally be 
// dynamically allocated by kmalloc
u8 buddy_alloc_init(struct mm_zone* zone)
{
    // Initialize the zone
    buddy_init_zone(zone);

    struct buddy_struct* buddy = (struct buddy_struct *)zone->alloc;

    buddy->used = 0;

    // Find the order of the buddy allocator
    u32 two_pwr_pages = round_down_power_two(zone->page_cnt);
    buddy->max_orders = __builtin_ctz(two_pwr_pages) + 1;
    
    // The map size is in number of words 
    u32 map_size = get_map_size(buddy->max_orders);

    u32* map = kmalloc(map_size * 4);
    if (map == NULL)
        return 0;

    // Zero the buddy bitmap 
    mem_set(map, 0, map_size * 4);

    // Allocate the buddy lists and map pointers 
    buddy->orders = (struct buddy_order *)
        kmalloc(sizeof(struct buddy_order) * buddy->max_orders);

    if (buddy->orders == NULL)
        return 0;

    for (u32 i = buddy->max_orders; i --> 0;) {
        // Assign the map pointer 
        if (i) {
            buddy->orders[i].map = map;
            map += get_order_map_words(i, buddy->max_orders);
        } else {
            buddy->orders[i].map = NULL;
        }

        // Initialize the buddy order list 
        list_init(&buddy->orders[i].free_list);
    }

    // When the buddy system is initialized it should contain one single block 
    // spanning the entire memory zone (power of two)
    list_add_first(&zone->start->node,
        &buddy->orders[buddy->max_orders - 1].free_list);
    
    return 1;
}

// Takes in the index of a block and returns the parent bit index
#define get_parent_bit(index, order) \
    ((index & ~((1 << (order + 1)) - 1)) >> (order + 1))

// Takes in the requested order and the allocator zone and gives a pointer to 
// the first page in that region
struct page* buddy_alloc_pages(u32 order, struct mm_zone* zone)
{
    u32 atomic = __atomic_enter();

    struct buddy_struct* buddy = (struct buddy_struct *)zone->alloc;

    struct buddy_order* curr_order_obj = buddy->orders + order;
    u32 curr_order = order;
    
    while (curr_order < buddy->max_orders) {
        if (list_is_empty(&curr_order_obj->free_list) == 0) {
            break;
        }
        curr_order++;
        curr_order_obj++;
    }

    if (curr_order >= buddy->max_orders) {
        __atomic_leave(atomic);
        panic("Buddy allocator out of memory");
        return NULL;
    }
    
    // curr_order holds the index of the order which is able to store the 
    // requested memory. This does NOT have to be a perfect fit
    struct list_node* new_node = list_get_first(&curr_order_obj->free_list);
    struct page* new_page = list_get_entry(new_node, struct page, node);

    // If the curr_order is NOT the max order toogle the parent bit 
    u32 page_index = new_page - zone->start;
    if (curr_order < (buddy->max_orders - 1)) {

        u32 bit = get_parent_bit(page_index, curr_order);
        toggle_bit(bit, (curr_order_obj + 1)->map);
    }

    // We have a order which can hold a block. If this is a perfect fit, we have 
    // to remove the block. If this is too big, we still have to remove the 
    // block - because we are splitting it. 
    list_delete_first(&curr_order_obj->free_list);

    if (order == curr_order) {
        goto alloc_ok;
    }

    // If the block is too big - but found - we are splitting the block until
    // the right size is obtained
    do {
        --curr_order;
        --curr_order_obj;

        // Toggle the parent 
        u32 bit = get_parent_bit(page_index, curr_order);
        toggle_bit(bit, (curr_order_obj + 1)->map);

        // Split the block by insert the buddy into the free list 
        struct page* _page = zone->start + page_index + (1 << curr_order);
        list_add_first(&_page->node, &buddy->orders[curr_order].free_list);

    } while (curr_order > order);

alloc_ok:
    // Update the size from the allocation 
    new_page->order = order;
    buddy->used += (1 << order) * 4096;
    __atomic_leave(atomic);
    return new_page;    
}

// Frees a page pointer allocated by the buddy allocator. This requires the
// the order to be specified
void buddy_free_pages(struct page* page, struct mm_zone* zone)
{
    u32 atomic = __atomic_enter();
    u32 order = page->order;
    u32 free_order = page->order;

    struct buddy_struct* buddy = (struct buddy_struct *)zone->alloc;
    struct buddy_order* curr_order = &buddy->orders[order];
    u32 index = page - zone->start;

    while (order < (buddy->max_orders - 1)) {
        u32 bit = get_parent_bit(index, order);
        u32 bit_val = get_bit(bit, buddy->orders[order + 1].map);

        // In any case toogle the bit. This will indicate that we are either
        // freeing the block or merging the buddies.
        toggle_bit(bit, buddy->orders[order + 1].map);  

        // If the parent bis is 0 it means that the free block we have is 
        // has a non-free buddy. They cannot be combined
        if (bit_val == 0) {
            break;
        }

        u32 buddy_index = index ^ (1 << order);
        struct list_node* n = &(zone->start + buddy_index)->node;

        list_delete_node(&(zone->start + buddy_index)->node);

        index &= ~((1 << (order + 1)) - 1);
        order++;
    }

    page = zone->start + index;
    list_add_first(&page->node, &buddy->orders[order].free_list);

    if (buddy->used < (1 << free_order) * 4096) {
        panic("Buddy error\n");
    }
    buddy->used -= (1 << free_order) * 4096;

    __atomic_leave(atomic);
}
