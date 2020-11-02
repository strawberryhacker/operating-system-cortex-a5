/// Copyright (C) strawberryhacker 

#ifndef SLOB_H
#define SLOB_H

#include <citrus/types.h>
#include <citrus/mm.h>

// Alignment and block size in number of bytes 
#define SLOB_ALIGN 8
#define SLOB_MIN_BLOCK 16

struct slob_node {
    u32 size;
    struct slob_node* next;
};

struct slob_stats {
    u32 used;
    u32 total;
};

// Memory region in kernel virtual memory => phys 
struct slob_struct {
    u32 start_addr;
    u32 end_addr;

    // Stats 
    struct slob_stats stats;

    struct slob_node node;
    struct slob_node* first_node;
    struct slob_node* last_node;
};

u8 slob_init(struct mm_zone* zone);
void* slob_alloc(u32 size, struct mm_zone* zone);
void slob_free(void* ptr, struct mm_zone* zone);

#endif
