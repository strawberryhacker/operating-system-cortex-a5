/// Copyright (C) strawberryhacker 

#include <cinnamon/slob.h>
#include <cinnamon/print.h>
#include <cinnamon/mm.h>
#include <stddef.h>

/// Initialized the SLOB allocator. The zone must be setup correctly
u8 slob_init(struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    // Find the absolute start and end address of the slob allocator space 
    u32 page_index = zone->start - mm_get_page_array();
    slob->start_addr = KERNEL_START + page_index * 0x1000;
    slob->end_addr = slob->start_addr + zone->page_cnt * 0x1000;

    // Sanity check 
    if (slob->start_addr > slob->end_addr) {
        return 0;
    }

    // Align the start and stop address to MM_ALIGN 
    if (slob->start_addr & (SLOB_ALIGN - 1)) {
        slob->start_addr += SLOB_ALIGN;
        slob->start_addr &= ~(SLOB_ALIGN - 1);
    }
    if (slob->end_addr & (SLOB_ALIGN - 1)) {
        slob->end_addr &= ~(SLOB_ALIGN - 1);
    }

    // Make the first and last node 
    struct slob_node* tmp_node = (struct slob_node *)slob->start_addr;
    slob->last_node = (struct slob_node *)slob->end_addr -
        sizeof(struct slob_node);

    // Setup the links and the size fields 
    slob->node.size = 0;
    slob->node.next = tmp_node;
    slob->first_node = &slob->node;

    tmp_node->size = (u8 *)slob->last_node - (u8 *)slob->first_node;
    tmp_node->next = slob->last_node;

    slob->last_node->next = NULL;
    slob->last_node->size = 0;

    // Update the mm stats 
    slob->stats.total = tmp_node->size;
    slob->stats.used = 0;

    return 1;
}

/// The `first` node is the node which is pointing to the first physical node in
/// the memory. The `last` is the last node pointing to NULL.  
static u8 slob_insert_free(struct slob_node* first, struct slob_node* last,
    struct slob_node* node)
{
    struct slob_node* it;

    for (it = first; it; it = it->next) {
        if ((u32)it->next > (u32)node) {
            break;
        }
    }
    if (it == NULL) {
        return 0;
    }

    // The `it` is pointing to the mm_node before the node to insert 
    u32 addr = (u32)it;
    if ((addr + it->size) == (u32)node) {
        // Backward merge 
        it->size += node->size;
        node = it;
    }

    addr = (u32)node;
    if ((addr + node->size) == (u32)it->next) {
        if (it->next != last) {
            // Forward merge 
            node->size += it->next->size;
            node->next = it->next->next;
        } else {
            // If backward merge this does not do anything 
            node->next = it->next;
        }
    } else {
        // If backboard merge this does not do anything 
        node->next = it->next;
    }

    if (node != it) {
        it->next = node;
    }
    return 1;
}

/// Extend the memory region covered by the SLOB allocator
void slob_extend(struct mm_zone* zone, u32 pages)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
    u32 prev_end = slob->end_addr;

    struct slob_node* it = slob->first_node;
    while (it->next != slob->last_node);

    // It is pointing to the node before the last node
    struct slob_node* tmp = slob->last_node;

    slob->end_addr += pages * 0x1000;

    slob->last_node = (struct slob_node *)
        (slob->end_addr - sizeof(struct slob_node));
    
    // This is the new last node
    slob->last_node->next = NULL;
    slob->last_node->size = 0;
    
    // The previous last node is now free
    it->next = slob->last_node;

    tmp->next = NULL;
    tmp->size = pages * 0x1000;

    // Merge any blocks if possible
    slob_insert_free(slob->first_node, slob->last_node, tmp);

    // Update the statistics
    slob->stats.total += pages * 0x1000;
}

/// Allocates a physically and virtually continous memory region. This is based 
/// on the SLOB allocator (simple list of block)
void* slob_alloc(u32 size, struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    if (size == 0) {
        return NULL;
    }

    size += sizeof(struct slob_node);

    // Align the size 
    if (size & (SLOB_ALIGN - 1)) {
        size += SLOB_ALIGN;
        size &= ~(SLOB_ALIGN - 1);
    }

    struct slob_node* it_prev = slob->first_node;
    struct slob_node* it = it_prev->next;

    while (it) {
        if (it->size >= size) {
            break;
        }
        it_prev = it;
        it = it->next;
    }
    if (it == NULL) {
        // We are out of memory 
        print("Dangit\n");
        return NULL;
    }

    // Remove the current node from the list 
    it_prev->next = it->next;

    // We have to check if the memory block is large enough to contain more data 
    u32 new_size = it->size - size;
    if (new_size >= SLOB_MIN_BLOCK) {
        /// We have more space, and the space is large enough to contain at least
        /// MM_MIN_BLOCK number of bytes
        struct slob_node* new = (struct slob_node *)((u32)it + size);

        // The kmalloc_insert_free requires the size field to be set 
        new->size = new_size;
        slob_insert_free(slob->first_node, slob->last_node, new);
    } else {
        size = it->size;
    }

    // Size is holding the number of bytes to be allocated 
    slob->stats.used += size;
    it->size = size;
    it->next =(struct slob_node *)0xC0DEBABE;

    return (void *)((u32)it + sizeof(struct slob_node));
}

void slob_free(void* ptr, struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    if (ptr == NULL) {
        return;
    }
    struct slob_node* free = (struct slob_node *)((u32)ptr - sizeof(struct slob_node));

    if ((u32)free->next != 0xC0DEBABE) {
        // This is not a pointer we track 
        return;
    }

    // Range check 
    if (((u32)free < (u32)slob->first_node) ||
        ((u32)free >= (u32)slob->last_node)) {
        return;
    }
    slob->stats.used -= free->size;
    slob_insert_free(slob->first_node, slob->last_node, free);
}

u32 slob_get_used(struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    return slob->stats.used;
}
