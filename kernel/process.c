/// Copyright (C) strawberryhacker

#include <cinnamon/types.h>
#include <cinnamon/thread.h>
#include <cinnamon/page_alloc.h>
#include <cinnamon/mm.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/align.h>
#include <cinnamon/list.h>
#include <cinnamon/page_alloc.h>
#include <cinnamon/pt_entry.h>
#include <cinnamon/mem.h>
#include <cinnamon/panic.h>
#include <cinnamon/cache.h>

/// This sets up the new process memory space and 
struct page* process_mm_init(struct thread* thread, u32 stack_size)
{
    // The init process has to dynamically allocte the mm struct
    struct mm_process* mm = (struct mm_process *)
        kzmalloc(sizeof(struct mm_process));

    mm_process_init(mm);
    thread->mm = mm;

    // Make the main level 1 page table
    struct page* lv1 = lv1_pt_alloc();
    mm_process_add_page(lv1, mm);
    mm->ttbr_phys = page_to_pa(lv1);

    return NULL;
}
