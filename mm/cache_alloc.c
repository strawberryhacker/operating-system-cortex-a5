#include <citrus/cache_alloc.h>
#include <citrus/boot_alloc.h>
#include <citrus/cache.h>
#include <citrus/align.h>
#include <citrus/panic.h>
#include <citrus/mm.h>
#include <stddef.h>

static u8* buffer;
static u8* end;
static u8* pos;

static volatile u8 enabled = 0;

void cache_alloc_init(void)
{
    pos = buffer = boot_alloc(0x100000, 0x100000);
    end = buffer + 0x100000;

    // Mark the 1M pages as non-cacheable. ASID matching is not nessecary 
    // bacause they're in kernel space
    struct ste_attr attr = {
        .access = STE_ACCESS_FULL_ACC,
        .mem = STE_MEM_NON_CACHE,
        .domain = 15,
        .nG = 0,
        .xn = 0
    };

    mm_change_kernel_pt_attr((u32 *)buffer, &attr);
    asm ("dsb");
    mm_tlb_invalidate();
    dcache_clean_invalidate();

    enabled = 1;
}

void* cache_alloc(u32 size, u32 align)
{
    assert(enabled);

    u8* new = pos;
    new = align_up_ptr(new, align);

    if (new + size > end)
        return NULL;
    
    // Alloction success
    pos = new + size;
    return new;
}
