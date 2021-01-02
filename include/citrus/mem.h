/// Copyright (C) strawberryhacker 

#ifndef MEM_H
#define MEM_H

#include <citrus/types.h>

void mem_dump(const void* mem, u32 size, u32 col, u8 hex);
void mem_set(void* ptr, u8 fill, u32 size);
void mem_copy(const void* src, void* dest, u32 size);
u32 mem_cmp(const void* src1, const void* src2, u32 size);

/// Functions for reading a writing data with endianess
u16 read_le16(const void* ptr);
u32 read_le32(const void* ptr);
u64 read_le64(const void* ptr);

u16 read_be16(const void* ptr);
u32 read_be32(const void* ptr);
u64 read_be64(const void* ptr);

void store_be32(u32 val, const void* ptr);
void store_be16(u16 val, const void* ptr);

#endif
