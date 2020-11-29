/// Copyright (C) strawberryhacker

#ifndef MM_H
#define MM_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <citrus/pt_entry.h>

#define DDR_SIZE 134217728
#define DDR_PAGES 32768

#define KERNEL_START 0x80000000
#define KERNEL_OFFSET 0x60000000

/// Converts a kernel virtual address to physical address
static inline void* va_to_pa(void* va)
{
    u8* va_ptr = (u8 *)va;
    va_ptr -= KERNEL_OFFSET;
    return va_ptr;
}

/// Converts a physical address to kernel virtual address
static inline void* pa_to_va(void* pa)
{
    u8* pa_ptr = (u8 *)pa;
    pa_ptr += KERNEL_OFFSET;
    return pa_ptr;
}

/// Main page descriptor. Keep this short
struct page {
    u32* mem_map;
    u32 order;
    struct list_node node;
};

/// Returns the kernel virtual base address for the page array continaing a
/// struct page for every physical page
struct page* mm_get_page_array(void);

/// Functions for converting between pages and virtual / physical addresses
void* page_to_va(struct page* page);
void* page_to_pa(struct page* page);
struct page* va_to_page(void* page_addr);
struct page* pa_to_page(void* page_addr);

// Describes a memory zone used by a allocator
struct mm_zone {

    // Page start and stop address
    struct page* start;
    u32 page_cnt;

    // Points to the internal allocator deployed
    void* alloc;

    // List node for both the zone list and the zone allocator list
    struct list_node zone_node;
    struct list_node alloc_node;

    // Functions for getting the statistics from the allocator
    u32 (*get_used)(struct mm_zone* zone);
    u32 (*get_free)(struct mm_zone* zone);
    u32 (*get_total)(struct mm_zone* zone);
};

/// Main mm structure used in the system. This will contain a list of all the
/// zones allocated
struct mm {
    struct list_node zones;

    // Buddy allocator zones
    struct list_node buddy_zones;

    // SLOB allocator zones
    struct list_node slob_zones;
};

void mm_init(void);

/// Allocation of page tables
struct page* lv1_pt_alloc(void);
struct page* lv2_pt_alloc(void);
void lv1_pt_free(struct page* page);
void lv2_pt_free(struct page* page);

/// Verious coprocessor accesses for page table management. Do NOT try to run
/// this in user mode. This will trigger a UNDEF exception
static inline void set_ttbr0(u32 paddr)
{
    asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r" (paddr));
    asm volatile ("isb" : : : "memory");
}

static inline void set_ttbr1(u32 paddr)
{
    asm volatile ("mcr p15, 0, %0, c2, c0, 1" : : "r" (paddr));
}

static inline u32 get_ttbr0(void)
{
    u32 paddr;
    asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (paddr));
    return paddr;
}

static inline u32 get_ttbr1(void)
{
    u32 paddr;
    asm volatile ("mrc p15, 0, %0, c2, c0, 1" : "=r" (paddr));
    return paddr;
}

static inline void mm_tlb_invalidate(void)
{
    asm volatile ("mcr p15, 0, r0, c8, c7, 0");
    asm volatile ("dmb" : : : "memory");
    asm volatile ("isb" : : : "memory");
}


/// Bitmap for the secondary level page table. Each page can store 3 secondary
/// level page tables and this bit maps indicated which page tables whithin the
/// page that are free. The first bit corresponds to l2 page table number 0 at
/// offset page_addr + 1024
struct pt2 {
    u32 bitmap;
};

/// Every new process allocated a struct proc_mm from the kernel malloc. This
/// will hold the memory space used by that process. Every child thread will
/// keep a point to this structure
struct mmap {
    // Base address of the physical address space going into TTBR0 (must be first)
    u32* ttbr_phys;

    // Virtual addresses to regions
    u32* data_s;
    u32* data_e;

    u32* heap_s;
    u32* heap_e;

    u32* stack_s;
    u32* stack_e;

    // List to keep track of all the pages alloated
    struct list_node page_list;
    u32 page_cnt;

    // The kernel uses one page to store 3 secondary page tables. This holds the
    // current page used for page table allocations. When a level 2 page table
    // is requested and this is full it gets replaced with new allocated page
    struct page* pt2_ptr;
};

/// Contains the attributes for a level 2 page table entry
struct pte_attr {
    enum pte_mem mem;
    enum pte_access access;
    u8 xn     : 1;
    u8 nG     : 1;
    u8 domain : 4;
};

/// Contains the attributes for a level 1 section table entry
struct ste_attr {
    enum ste_mem mem;
    enum ste_access access;
    u8 xn     : 1;
    u8 nG     : 1;
    u8 domain : 4;
};

void lv2_pt_init(struct page* page);
u32* lv2_pt_find_in_page(struct page* page);


/// Adds a new allocated page to the memory manager. This will be placed in a
/// linked list, so that we can kill a process and free the memory
static inline void mm_process_add_page(struct page* page, struct mmap* mm)
{
    list_add_first(&page->node, &mm->page_list);
}

void mm_process_init(struct mmap* mm);

u8 mm_map_in_pages(struct mmap* mm, struct page* page, u32 page_cnt,
    u32 virt_addr, struct pte_attr* attr);

u32* set_break(u32 bytes);

/// Functions for gettting the total amount of allocated memory
u32 mm_get_total_used(void);
u32 mm_get_total(void);

// Functions for changing the attributes of a 1M page in the kernel page-table
void mm_change_kernel_pt_attr(u32* virt_addr, struct ste_attr* attr);

#endif
