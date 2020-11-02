/// Copyright (C) strawberryhacker

#ifndef FS_CACHE_H
#define FS_CACHE_H

#include <citrus/types.h>

#define FS_CACHE_SIZE (1048576)

struct fs_cache {
    // Private info
    struct fs_cache_line* cache;

    u8 (*read)(u32 page, u8* buffer);
    u8 (*fault)(u32 page);
};

struct fs_cache_line {
    u32 tag;
    u8 dirty;
    u8 valid;

    // Page cache
    u8 cache[512];
};

struct fs_cache* create_fs_cache(u32 order, void (*read)(u32), 
    void (*write)(u32));

struct 

#endif
