// Copyright (C) strawberryhacker 

#include <citrus/mm.h>
#include <citrus/align.h>
#include <citrus/kmalloc.h>
#include <citrus/slob.h>
#include <citrus/buddy_alloc.h>
#include <citrus/mem.h>
#include <citrus/boot_alloc.h>
#include <citrus/panic.h>
#include <citrus/sched.h>
#include <citrus/thread.h>
#include <citrus/cache.h>
#include <citrus/panic.h>
#include <citrus/lcd.h>
#include <citrus/cache_alloc.h>

// Hold the virtual end address of the kernel memory. Defined in the linker
extern u32 _kernel_e;

#define MM_ZONE_CNT 2

// Allocate the memory zones
struct mm_zone zones[MM_ZONE_CNT];

// Allocate the main memory manager object
struct mm mm;

// Allocate one kernel SLOB allocator
struct slob_struct slob_allocator;
struct buddy_struct buddy_allocator;

// Holds the pointer to the base address of the page array
struct page* page_array;

// Returns the kernel virtual address to the page array
struct page* mm_get_page_array(void)
{
    return page_array;
}

// Set up the page array using the boot_allocator
static void mm_setup_page_array(void)
{
    // We need to allocate one struct page for every physical page inthe system.
    // This should be aligned with a page
    page_array = (struct page *)
        boot_alloc(sizeof(struct page) * DDR_PAGES, 4096);

    // Initialize the page structures
    for (u32 i = 0; i < DDR_PAGES; i++) {
        list_node_init(&page_array[i].node);
    }

    // Delete the memory
    u32 addr = boot_alloc_get_end_vaddr();
    u32* ptr = align_up_ptr((void *)addr, 4);
}

// Starts up the custom allocators; binary buddy alloc for pages and the SLOB
// list allocator for the kernel allocations
void mm_allocators_init(void)
{
    // Find the first page_structure which is available for allocation
    u32 kernel_used = boot_alloc_get_end_vaddr() - KERNEL_START;

    // Find the number of pages in the kernel used space
    if (kernel_used & 0xFFF) {
        kernel_used += 4096;
        kernel_used &= ~0xFFF;
    }
    u32 kernel_pages = kernel_used / 4096;

    // Find how much memory should be partitioned into the buddy allocator
    u32 buddy_pages = DDR_PAGES / 3;
    buddy_pages = round_up_power_two(buddy_pages);

    // SLOB allocator size
    u32 slob_pages = DDR_PAGES - kernel_pages - buddy_pages;

    // Make the kmalloc zone
    zones[0].alloc = &slob_allocator;
    zones[0].start = page_array + kernel_pages;
    zones[0].page_cnt = slob_pages;
    assert(slob_init(zones));

    // Make the buddy allocator
    buddy_allocator.max_orders = 15;  // Fix this
    zones[1].alloc = &buddy_allocator;
    zones[1].start = page_array + kernel_pages + slob_pages;
    zones[1].page_cnt = buddy_pages;
    assert(buddy_alloc_init(zones + 1));
}

// Iterates trough all the zones and returns the number of used bytes by the
// allocators
u32 mm_get_total_used(void)
{
    u32 used = 0;
    for (u32 i = 0; i < MM_ZONE_CNT; i++) {
        struct mm_zone* zone = &zones[i];

        if (zone->get_used) {
            used += zone->get_used(zone);
        }
    }

    return used;
}

// Iterates trough all the zones and returns the number of total bytes
// available for allocation
u32 mm_get_total(void)
{
    u32 total = 0;
    for (u32 i = 0; i < MM_ZONE_CNT; i++) {
        struct mm_zone* zone = &zones[i];

        if (zone->get_total) {
            total += zone->get_total(zone);
        }
    }

    return total;
}

static void mm_struct_init(struct mm* mm)
{
    list_init(&mm->zones);
    list_init(&mm->buddy_zones);
    list_init(&mm->slob_zones);
}

// Early memory manager init routine
static void mm_early_init(void)
{
    boot_alloc_init();
    mm_setup_page_array();

    mm_struct_init(&mm);
}

// This sets up the main allocator for use in the kernel space. The buddy
// allocator will be used for the ex_heap call to the applications.
void mm_init(void)
{
    // Make sure the memory is setup correctly
    mm_early_init();

    // Call the display subsystem to allocate buffers for the display. This is
    // done here because it needs to be non-cacheable and allocate several MB
    lcd_layers_alloc();

    // Add a temporarily cache allocator here
    cache_alloc_init();

    // Retire the boot allocator before the main allocators are enabled
    boot_alloc_retire();
    mm_allocators_init();
}

// Allocates a number of bytes for use by the kernel. This will return a 
// kernel virtual address
void* kmalloc(u32 size)
{
    return slob_alloc(size, zones);
}

// Allocates a number of bytes for use by the kernel. This will return a 
// kernel virtual address pointer to zeroed memory
void* kzmalloc(u32 size) 
{
    u32* ptr = slob_alloc(size, zones);
    mem_set(ptr, 0, size);
    return ptr;
}

// Free a pointer allocated with the SLOB allocator
void kfree(void* ptr)
{
    slob_free(ptr, zones);
}

// Allocates a number of pages from the binary buddy using the given order.
// This will allocate 2 ** order number of pages. Returns a pointer to a
// virtual page structure
struct page* alloc_pages(u32 order)
{
    return buddy_alloc_pages(order, zones + 1);
}

// Allocates one page from the binary buddy. Returns a pointer to a virtual 
// page structure
struct page* alloc_page(void)
{
    return buddy_alloc_pages(0, zones + 1);
}

// Frees one or more pages. The size is contains within internal structures
void free_pages(struct page* page)
{
    buddy_free_pages(page, zones + 1);
}

// Converts a number of bytes to an allocation order for the binary buddy
// allocator
u32 bytes_to_order(u32 bytes)
{
    u32 pages = (u32)align_up_ptr((void *)bytes, 4096) / 4096;

    return __builtin_ctz(round_up_power_two(pages));
}

// Converts a number of pages to an allocation order for the binary buddy
// allocator
u32 pages_to_order(u32 pages)
{
    return __builtin_ctz(round_up_power_two(pages));
}

// Allocates a level 1 page table. Due the the kernel 2GB:2GB split this page
// table is 2048 bytes and not 4096. Thus it will only cover 2BG of address
// space. This returns a virtual page struct address
struct page* lv1_pt_alloc(void)
{
    struct page* page = buddy_alloc_pages(1, zones + 1);
    mem_set(page_to_va(page), 0, 4096 * 2);
    return page;
}

// Allocates a level 2 page table. This is 4096 bytes in size. This returns a
// virtual page struct address
struct page* lv2_pt_alloc(void)
{
    struct page* page = buddy_alloc_pages(0, zones + 1);
    mem_set(page_to_va(page), 0, 4096);
    return page;
}

// Converts a page address to a kernel virtual address
void* page_to_va(struct page* page)
{
    u32 index = page - page_array;
    return (void *)(KERNEL_START + (index * 4096));
}

// Converts a page address to a phyiscal address
void* page_to_pa(struct page* page)
{
    u32 index = page - page_array;
    u32 vaddr = KERNEL_START + (index * 4096);

    return va_to_pa((void *)vaddr);
}

// Converts a kernel virtual address to a virtual page address
struct page* va_to_page(void* page_addr)
{
    if ((u32)page_addr & 0xFFF) {
        return NULL;
    }

    u32 page_index = ((u32)page_addr - KERNEL_START) / 4096;
    return page_array + page_index;
}

// Converts a physical address to a virtual page address
struct page* pa_to_page(void* page_addr)
{
    u32 vaddr = (u32)pa_to_va(page_addr);
    if (vaddr & 0xFFF) {
        return NULL;
    }

    u32 page_index = (vaddr - KERNEL_START) / 4096;
    return page_array + page_index;
}

// Initializes the page used for secondary level page tables. One page contains
// three level 2 page tables. A pt2 structure is place in the first 1 KiB of
// the page. This will contain info about the status of the page tables
void lv2_pt_init(struct page* page)
{
    struct pt2* pt = (struct pt2 *)page_to_va(page);
    pt->bitmap = 0xFFFFFFFF;
}

// Initializes a thread_mm structure. This is allocated per process basis and
// contains info about the process memory map
void mm_process_init(struct mmap* mm)
{
    list_init(&mm->page_list);
    mm->pt2_ptr = NULL;

    mm->data_e = 0;
    mm->data_s = 0;

    // Set the inital stack region
    mm->stack_s = (u32 *)KERNEL_START;
    mm->stack_e = (u32 *)KERNEL_START;

    // The heap pointers are initialized to zero
    mm->heap_s = 0;
    mm->heap_e = 0;
}

// Returns the PTE entry value based on the physical address of the page and
// the page table attibutes
static inline u32 mm_get_pte(u32 phys_addr, const struct pte_attr* attr)
{
    u32 pte = attr->access | attr->mem | PTE_MASK;

    // Execute never
    if (attr->xn) {
        pte |= PTE_XN;
    }

    // Not global
    if (attr->nG) {
        pte |= PTE_nG;
    }

    // Physical base address of the page
    pte |= PTE_BASE(phys_addr);

    return pte;
}

// Returns the STE entry value based on the physical address of the section and
// the page table attibutes
static inline u32 mm_get_ste_sect(u32 phys_addr, const struct ste_attr* attr)
{
    u32 ste = attr->access | attr->mem | STE_SECTION_MASK |
        STE_SECTION_DOMAIN(attr->domain);
    // Execute never
    if (attr->xn) {
        ste |= STE_SECTION_XN;
    }

    // Not global
    if (attr->nG) {
        ste |= STE_SECTION_nG;
    }

    // Physical base address of the page
    ste |= STE_SECTION_BASE(phys_addr);

    return ste;
}

// Returns the STE entry value for a level 2 page table pointer. It takes in 
// the physical address of the level 2 page table as well as the domain
static inline u32 mm_get_ste_ptr(u32 phys_addr, u8 domain)
{
    assert(domain < 16);
    return STE_PTR_BASE(phys_addr) | STE_PTR_DOMAIN(domain) | STE_PTR_MASK;
}

// Maps in a level 2 page table in the memory space pointed to by ttbr. The
// phys_addr hold the physical address of the page table and the virt_address
// holds the virtual address of the target address
static void mm_map_in_pt(u32* ttbr_virt, u32 phys_addr, u32 virt_addr, u8 domain)
{
    // The virtual address must be aligned to a 1 MiB boundary
    assert((virt_addr & 0xFFFFF) == 0);

    // The physical base address must be aligned to a 1 KiB boundary
    assert((phys_addr & 0x3FF) == 0);

    u32 ste = mm_get_ste_ptr(phys_addr, domain);

    // Map in the page table
    ttbr_virt[virt_addr >> 20] = ste;
}

// Maps in a page into the memory space pointed to by ttbr. The phys_addr holds
// the the physical address of the page and the virt_addr holds the target 
// virtual address. This is unsafe because the level 2 page table has to exist
static void mm_map_in_page_unsafe(u32* ttbr_virt, u32 phys_addr, u32 virt_addr,
    struct pte_attr* attr)
{

    // The virtual and physical address must be aligned to a 4 KiB boundary
    assert((virt_addr & 0xFFF) == 0);
    assert((phys_addr & 0xFFF) == 0);

    // Get the physical base of the secondary level page table
    u32* pt2_phys = (u32 *)STE_PTR_BASE(ttbr_virt[virt_addr >> 20]); 
    assert(pt2_phys);

    // Get virtual address of the secondary level page table
    u32* pt2_virt = pa_to_va(pt2_phys);

    pt2_virt[(virt_addr >> 12) & 0xFF] = mm_get_pte(phys_addr, attr);
}

// Returns if the STE entry is empty (fault)
static inline u8 mm_is_ste_empty(u32 ste)
{
    return ((ste & 0b11) == 0);
}

// Check if the level 1 page table has a valid level 2 page page table mapping
// at virt_addr 
static inline u8 mm_has_ste_ptr_mapping(u32* ttbr_virt, u32 virt_addr)
{
    u32 ste = ttbr_virt[virt_addr >> 20];

    return (mm_is_ste_empty(ste)) ? 0 : 1;
}

// Returns a new level 2 page table. This might require a page allocation 
// which will store the new page within the mm->pt2_ptr. This returns the 
// virtual address of the new level 2 page table
static u32* mm_get_l2_pt(struct mmap* mm)
{
    assert(mm);

    if (mm->pt2_ptr) {
        // Check if it can hold another one
        struct pt2* pt = page_to_va(mm->pt2_ptr);

        for (u32 i = 0; i < 3; i++) {
            if (pt->bitmap & (1 << i)) {   
                pt->bitmap |= (1 << i);

                // If a bit is free return the address
                return (u32 *)((u8 *)pt + 1024 * (i + 1));
            }
        }
    }

    // The mm->pt2_ptr is either NULL or full. Allocate a new page
    struct page* pt2 = lv2_pt_alloc();

    if (pt2 == NULL) {
        return NULL;
    }

    // Add the page to the page list
    mm->pt2_ptr = pt2;
    mm_process_add_page(pt2, mm);

    return (u32 *)((u8 *)page_to_va(pt2) + 1024);
}

// Maps in a number of pages into the virtual address space specified by ttbr.
// This takes in the virtual address that the pages should be mapped to. It 
// returns 1 if success and 0 if an alocation failure has occured
u8 mm_map_in_pages(struct mmap* mm, struct page* page, u32 page_cnt, 
    u32 virt_addr, struct pte_attr* attr)
{
    // The virtual address must be aligned at a 4 KiB boundary
    assert((virt_addr & 0xFFF) == 0);

    u32* ttbr_virt = pa_to_va(mm->ttbr_phys);
    while (page_cnt--) {

        // Check if the first level page table has a valid mapping
        if (mm_has_ste_ptr_mapping(ttbr_virt, virt_addr) == 0) {

            // Map in a new level 2 page table
            u32* pt2_virt = mm_get_l2_pt(mm);

            // Allocation failed
            if (pt2_virt == NULL) {
                return 0;
            }

            u32 pt2_phys = (u32)va_to_pa(pt2_virt);
            mm_map_in_pt(ttbr_virt, pt2_phys, virt_addr & ~0xFFFFF, attr->domain);
        }

        // We now have a level 2 page table mapping so we can map in the pages
        u32 page_phys = (u32)page_to_pa(page);
        mm_map_in_page_unsafe(ttbr_virt, page_phys, virt_addr, attr);

        page++;
        virt_addr += 4096;
    }

    dcache_clean();
    icache_invalidate();

    return 1;
}

// Extends the heap limit in a user process memory space. This will move the 
// heap break up.
u32* set_break(u32 bytes)
{
    struct mmap* mm = get_curr_mm_process();

    if (mm->heap_e == 0) {

        // Heap is not mapped
        mm->heap_s = mm->data_e;

        // Align the addresses with a page
        align_up_ptr(mm->heap_s, 4096);
        mm->heap_e = mm->heap_s;
    }

    if (bytes == 0) {
        return mm->heap_e;
    }

    u32 pages = align_up(bytes, 4096) / 4096;
    u32 order = pages_to_order(pages);

    struct page* page_ptr = alloc_pages(order);

    if (!page_ptr) {
        return mm->heap_e;
    }

    curr_thread_add_pages(page_ptr, 1 << order);

    struct pte_attr attr = {
        .access = PTE_ACCESS_FULL_ACC,
        .mem    = PTE_MEM_WRITE_THROUGH,
        .domain = 15,
        .nG     = 0,
        .xn     = 0
    };

    assert(mm_map_in_pages(mm, page_ptr, 1 << order, (u32)mm->heap_e, &attr));

    // Extend the heap region
    mm->heap_e += 4096 * (1 << order);

    return mm->heap_e;
}

extern u32 _early_kernel_lv1_pt_s;

// Returns the start address of the kernel page table
static inline u32* mm_get_kernel_pt(void)
{
    return (u32 *)&_early_kernel_lv1_pt_s;
}

// Changes the default attributes of the kernel page table. This only operates
// on first level page tables. This might be used to mark some regions as 
// non-cacheable
void mm_change_kernel_pt_attr(u32* virt_addr, struct ste_attr* attr)
{
    assert(((u32)virt_addr & 0xFFFFF) == 0);

    // Grab the formatted entry
    u32 entry = mm_get_ste_sect((u32)va_to_pa(virt_addr), attr);
    u32* kernel_pt = mm_get_kernel_pt();

    // Update the entry
    kernel_pt[(u32)virt_addr >> 20] = entry;
    asm volatile ("dmb");
    mm_tlb_invalidate();
}
