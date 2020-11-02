/// Copyright (C) strawberryhacker 

#ifndef CACHE_H
#define CACHE_H

#include <citrus/types.h>

extern void icache_enable(void);
extern void icache_disable(void);
extern void icache_invalidate(void);

extern void dcache_enable(void);
extern void dcache_disable(void);
extern void dcache_clean(void);
extern void dcache_clean_range(u32 start, u32 end);
extern void dcache_invalidate(void);
extern void dcache_invalidate_range(u32 start, u32 end);
extern void dcache_clean_invalidate(void);
extern void dcache_clean_invalidate_range(u32 start, u32 end);

#endif
