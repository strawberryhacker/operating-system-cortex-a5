// Copyright (C) strawberryhacker

#include <citrus/boot_alloc.h>
#include <citrus/align.h>
#include <citrus/panic.h>
#include <citrus/print.h>

static u32 boot_alloc_en = 0;

// From the linker and defines where the boot alloctor can start allocating
extern u32 _kernel_e;

// This is pointing to the top of the current boot alloc space
static u8* boot_alloc_ptr;

// Enables the early boot allocator
void boot_alloc_init(void)
{
    boot_alloc_ptr = (u8 *)&_kernel_e;
    boot_alloc_en = 1;
}

// Allocates memory from the boot allocator region at the end of the linker
void* boot_alloc(u32 size, u32 align)
{
    if (!boot_alloc_en) {
        panic("Boot allocator retired");
    }

    // Align the start address
    boot_alloc_ptr = align_up_ptr(boot_alloc_ptr, align);

    void* ret_ptr = boot_alloc_ptr;
    boot_alloc_ptr += size;

    return ret_ptr;
}

void boot_alloc_retire(void)
{
    boot_alloc_en = 0;
}

// This should only be called after the boot_alloc_retire and will return the
// end of the boot_alloc region in virtual memory
u32 boot_alloc_get_end_vaddr(void)
{
    return (u32)boot_alloc_ptr;
}
