// Copyright (C) strawberryhacker 

#include <citrus/mem.h>
#include <citrus/print.h>

// Function for dumping a memory segment to the serial console
void mem_dump(const void* mem, u32 size, u32 col, u8 hex)
{
    const u8* src = (const u8 *)mem;
    
    for (u32 i = 0; size--;) {
        if (hex) {
            print("%02x ", src[i]);
        } else {
            print("%c", src[i]);
        }

        if ((++i % col) == 0) {
            print("\n");
        }
    }
}

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

u16 read_le16(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u16 val = 0;
    
    val |= src[0] << 0;
    val |= src[1] << 8;

    return val;
}


u32 read_le32(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u32 val = 0;

    val |= src[0] << 0;
    val |= src[1] << 8;
    val |= src[2] << 16;
    val |= src[3] << 24;

    return val;
}

u64 read_le64(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u64 val = 0;

    val |= (u64)src[0] << 0;
    val |= (u64)src[1] << 8;
    val |= (u64)src[2] << 16;
    val |= (u64)src[3] << 24;
    val |= (u64)src[4] << 32;
    val |= (u64)src[5] << 40;
    val |= (u64)src[6] << 48;
    val |= (u64)src[7] << 56;

    return val;
}

u16 read_be16(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u16 val = 0;
    
    val |= src[0] << 8;
    val |= src[1] << 0;

    return val;
}


u32 read_be32(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u32 val = 0;

    val |= src[0] << 24;
    val |= src[1] << 16;
    val |= src[2] << 8;
    val |= src[3] << 0;

    return val;
}

u64 read_be64(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u64 val = 0;

    val |= (u64)src[0] << 56;
    val |= (u64)src[1] << 48;
    val |= (u64)src[2] << 40;
    val |= (u64)src[3] << 32;
    val |= (u64)src[4] << 24;
    val |= (u64)src[5] << 16;
    val |= (u64)src[6] << 8;
    val |= (u64)src[7] << 0;

    return val;
}

void store_be16(u16 val, const void* ptr)
{
    u8* src = (u8 *)ptr;
    
    src[0] = (val >> 8) & 0xFF;
    src[1] = (val >> 0) & 0xFF;
}

void store_be32(u32 val, const void* ptr)
{
    u8* src = (u8 *)ptr;

    src[0] = (val >> 24) & 0xFF;
    src[1] = (val >> 16) & 0xFF;
    src[2] = (val >> 8 ) & 0xFF;
    src[3] = (val >> 0 ) & 0xFF;
}
