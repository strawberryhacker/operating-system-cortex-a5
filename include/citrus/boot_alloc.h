/// Copyright (C) strawberryhacker

#ifndef BOOT_ALLOC_H
#define BOOT_ALLOC_H

#include <citrus/types.h>

/// The boot allocator is a really simple allocator for managing allocation 
/// before the main allocators are started. This will retire when the other
/// allocators are enabled

void boot_alloc_init(void);
void* boot_alloc(u32 size, u32 align);
void boot_alloc_retire(void);
u32 boot_alloc_get_end_vaddr(void);

#endif
