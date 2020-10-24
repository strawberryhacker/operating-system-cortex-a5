/// Copyright (C) strawberryhacker 

#include <cinnamon/mem.h>

void mem_set(void* ptr, u8 fill, u32 size)
{
    u32 wfill = (fill << 24) | (fill << 16) | (fill << 8) | fill;
    u32 wsize = ((size & ~(4 - 1)) >> 2);
    u32 bsize = size & (4 - 1);

    u32* ptr_w = (u32 *)ptr;
    while (wsize--) {
        *ptr_w++ = wfill;
    }
    u8* ptr_b = (u8 *)ptr_w;
    while (bsize--) {
        *ptr_b++ = fill;
    }
}

void mem_copy(const void* src, void* dest, u32 size)
{
    u32 wsize = ((size & ~(4 - 1)) >> 2);
    u32 bsize = size & (4 - 1);

    const u32* src_w = (const u32 *)src;
    u32* dest_w = (u32 *)dest;

    while (wsize--) {
        *dest_w++ = *src_w++;
    }
    const u8* src_b = (const u8 *)src_w;
    u8* dest_b = (u8 *)dest_w;

    while (bsize--) {
        *dest_b++ = *src_b++;
    }
}

u32 mem_cmp(const void* src1, const void* src2, u32 size)
{
    const u8* srca = (const u8 *)src1;
    const u8* srcb = (const u8 *)src2;

    while (size--) {
        if (*srca++ != *srcb++) {
            return 0;
        }
    }
    return 1;
}
