/// Copyright (C) strawberryhacker 

#ifndef MEM_H
#define MEM_H

#include <cinnamon/types.h>

void mem_set(void* ptr, u8 fill, u32 size);

void mem_copy(const void* src, void* dest, u32 size);

#endif
