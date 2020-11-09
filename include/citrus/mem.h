/// Copyright (C) strawberryhacker 

#ifndef MEM_H
#define MEM_H

#include <citrus/types.h>

void mem_set(void* ptr, u8 fill, u32 size);

void mem_copy(const void* src, void* dest, u32 size);

u32 mem_cmp(const void* src1, const void* src2, u32 size);

u32 mem_read_le32(const void* data);

#endif
