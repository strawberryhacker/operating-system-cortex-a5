// Copyright (C) strawberryhacker 

#include <citrus/slob.h>
#include <citrus/print.h>
#include <citrus/mm.h>
#include <citrus/atomic.h>
#include <citrus/panic.h>
#include <stddef.h>

// Gets the total number of bytes free
static u32 slob_get_free(struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
    return slob->stats.total - slob->stats.used;
}

// Gets the total number of bytes used
static u32 slob_get_used(struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
    return slob->stats.used;
}

// Gets the total number of bytes in total
static u32 slob_get_total(struct mm_zone* zone)
{
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
    return slob->stats.total;
}

// Initailzie the zone structure used for the buddy allocator
static void slob_init_zone(struct mm_zone* zone)
{
    zone->get_used = slob_get_used;
    zone->get_total = slob_get_total;
    zone->get_free = slob_get_free;
}

// Initialized the SLOB allocator. The zone must be setup correctly
u8 slob_init(struct mm_zone* zone)
{
    // Initiaize the zone
    slob_init_zone(zone);

    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    // Find the absolute start and end address of the slob allocator space 
    u32 page_index = zone->start - mm_get_page_array();
    slob->start_addr = KERNEL_START + page_index * 4096;
    slob->end_addr = slob->start_addr + zone->page_cnt * 4096;

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

// The `first` node is the node which is pointing to the first physical node in
// the memory. The `last` is the last node pointing to NULL.  
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

// Extend the memory region covered by the SLOB allocator
void slob_extend(struct mm_zone* zone, u32 pages)
{
    u32 atomic = __atomic_enter();

    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
    u32 prev_end = slob->end_addr;

    struct slob_node* it = slob->first_node;
    while (it->next != slob->last_node);

    // It is pointing to the node before the last node
    struct slob_node* tmp = slob->last_node;

    slob->end_addr += pages * 4096;

    slob->last_node = (struct slob_node *)
        (slob->end_addr - sizeof(struct slob_node));
    
    // This is the new last nod
    slob->last_node->next = NULL;
    slob->last_node->size = 0;
    
    // The previous last node is now free
    it->next = slob->last_node;

    tmp->next = NULL;
    tmp->size = pages * 4096;

    // Merge any blocks if possible
    slob_insert_free(slob->first_node, slob->last_node, tmp);

    // Update the statistics
    slob->stats.total += pages * 4096;

    // Update the ZONE
    zone->page_cnt += pages;

    __atomic_leave(atomic);
}

// Allocates a physically and virtually continous memory region. This is based 
// on the SLOB allocator (simple list of block)
void* slob_alloc(u32 size, struct mm_zone* zone)
{
    if (size == 0) {
        return NULL;
    }

    // Due to kernel thread concurrency the memory allocation must disable irq
    u32 atomic = __atomic_enter();

    struct slob_struct* slob = (struct slob_struct *)zone->alloc;
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
        __atomic_leave(atomic);
        return NULL;
    }

    // Remove the current node from the list 
    it_prev->next = it->next;

    // We have to check if the memory block is large enough to contain more data 
    u32 new_size = it->size - size;
    if (new_size >= SLOB_MIN_BLOCK) {
        // We have more space, and the space is large enough to contain at least
        // MM_MIN_BLOCK number of bytes
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

    // Restore the irq state
    __atomic_leave(atomic);
    return (void *)((u32)it + sizeof(struct slob_node));
}

// Frees a pointer allocated by slob_alloc
void slob_free(void* ptr, struct mm_zone* zone)
{
    u32 atomic = __atomic_enter();
    struct slob_struct* slob = (struct slob_struct *)zone->alloc;

    if (ptr == NULL) {
        panic("NULL pointer freed!");
    }
    struct slob_node* free = (struct slob_node *)((u32)ptr - sizeof(struct slob_node));

    // Check if the kmalloc tracks this pointer
    if ((u32)free->next != 0xC0DEBABE) {
        panic("Non-tracked pointer freed!");
    }

    // Range check 
    if (((u32)free < (u32)slob->first_node) ||
        ((u32)free >= (u32)slob->last_node)) {
        panic("kmalloc free error!");
    }

    slob->stats.used -= free->size;
    slob_insert_free(slob->first_node, slob->last_node, free);

    __atomic_leave(atomic);
}
