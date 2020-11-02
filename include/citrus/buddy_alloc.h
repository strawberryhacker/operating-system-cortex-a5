
/// Copyright (C) strawberryhacker 

#ifndef BUDDY_PAGE_ALLOC_H
#define BUDDY_PAGE_ALLOC_H

#include <citrus/types.h>
#include <citrus/mm.h>

/// The buddy system will contain one of these blocks per buddy order. Double the 
/// memory means adding a block
struct buddy_order {
    u32* map;
    struct list_node free_list;
};

/// This holds all information about the buddy alocator
struct buddy_struct {
    struct buddy_order* orders;
    u32 max_orders;

    // Memory stats fields
    u32 used;
};

u8 buddy_alloc_init(struct mm_zone* zone);
struct page* buddy_alloc_pages(u32 order, struct mm_zone* zone);
void buddy_free_pages(struct page* page, struct mm_zone* zone);

#endif
