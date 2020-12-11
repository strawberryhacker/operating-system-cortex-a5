#ifndef CACHE_ALLOC_H
#define CACHE_ALLOC_H

#include <citrus/types.h>

// Allocate only allocator for non-cacheable buffers

// Must be called in early mm init before the boot allocator retires
void cache_alloc_init(void);

void* cache_alloc(u32 size, u32 align);

#endif
