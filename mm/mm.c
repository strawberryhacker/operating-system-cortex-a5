/// Copyright (C) strawberryhacker 

#include <cinnamon/mm.h>
#include <cinnamon/align.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/slob.h>
#include <cinnamon/buddy_alloc.h>
#include <cinnamon/mem.h>
#include <cinnamon/boot_alloc.h>
#include <cinnamon/panic.h>
#include <cinnamon/pt_entry.h>
#include <cinnamon/sched.h>
#include <cinnamon/thread.h>

extern u32 _kernel_e;

#define MM_ZONE_CNT 2

/// Allocate the memory zones
struct mm_zone zones[MM_ZONE_CNT];

/// Allocate the main memory manager object
struct mm mm;

/// Allocate one kernel SLOB allocator
struct slob_struct slob_allocator;
struct buddy_struct buddy_allocator;

/// Holds the pointer to the base address of the page array
struct page* page_array;

/// Returns the kernel virtual address to the page array
struct page* mm_get_page_array(void)
{
    return page_array;
}

/// Set up the page array using the boot_allocator
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
    u32* ptr = align_up((void *)addr, 4);
}

/// Starts up the custom allocators; binary buddy alloc for pages and the SLOB
/// list allocator for the kernel allocations
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
    if (!slob_init(zones)) {
        print("cant initialize SLOB allocator\n");
    }

    // Make the buddy allocator
    buddy_allocator.max_orders = 15;  // Fix this
    zones[1].alloc = &buddy_allocator;
    zones[1].start = page_array + kernel_pages + slob_pages;
    zones[1].page_cnt = buddy_pages;
    if (!buddy_alloc_init(zones + 1)) {
        print("cant initialize buddy allocator\n");
    }
}

/// Iterates trough all the zones and returns the number of used bytes by the
/// allocators
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

/// Iterates trough all the zones and returns the number of total bytes
/// available for allocation
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

/// Early memory manager init routine
static void mm_early_init(void)
{
    boot_alloc_init();
    mm_setup_page_array();
}

/// This sets up the main allocator for use in the kernel space. The buddy
/// allocator will be used for the ex_heap call to the applications.
void mm_init(void)
{
    // Make sure the memory is setup correctly
    mm_early_init();

    // Retire the boot allocator before the main allocators are enabled
    boot_alloc_retire();

    mm_allocators_init();
}

/// Wrappers for the kernel malloc SLOB allocator
void* kmalloc(u32 size)
{
    return slob_alloc(size, zones);
}

void* kzmalloc(u32 size) 
{
    u32* ptr = slob_alloc(size, zones);
    mem_set(ptr, 0, size);
    return ptr;
}

void kfree(void* ptr)
{
    slob_free(ptr, zones);
}

/// Wrappers for the page allocation using binary buddy
struct page* alloc_pages(u32 order)
{
    return buddy_alloc_pages(order, zones + 1);
}

struct page* alloc_page(void)
{
    return buddy_alloc_pages(0, zones + 1);
}

void free_pages(struct page* page, u32 order)
{
    buddy_free_pages(page, order, zones + 1);
}

void free_page(struct page* page)
{
    buddy_free_pages(page, 0, zones + 1);
}

/// Getting the lowest possible order for a binary buddy allocation
u32 bytes_to_order(u32 bytes)
{
    u32 pages = (u32)align_up((void *)bytes, 4096) / 4096;

    return __builtin_ctz(round_up_power_two(pages));
}

u32 pages_to_order(u32 pages)
{
    return __builtin_ctz(round_up_power_two(pages));
}

/// Page table allocation wrappers
struct page* lv1_pt_alloc(void)
{
    struct page* page = buddy_alloc_pages(1, zones + 1);
    mem_set(page_to_va(page), 0, 0x2000);
    return page;
}

struct page* lv2_pt_alloc(void)
{
    struct page* page = buddy_alloc_pages(0, zones + 1);
    mem_set(page_to_va(page), 0, 4096);
    return page;
}

void lv1_pt_free(struct page* page)
{
    buddy_free_pages(page, 1, zones + 1);
}

void lv2_pt_free(struct page* page)
{
    buddy_free_pages(page, 0, zones + 1);
}

/// Gets the kernel virtual address from a page address
void* page_to_va(struct page* page)
{
    u32 index = page - page_array;
    return (void *)(KERNEL_START + (index * 4096));
}

/// Gets the physical address from a page address
void* page_to_pa(struct page* page)
{
    u32 index = page - page_array;
    u32 vaddr = KERNEL_START + (index * 4096);

    return _pa((void *)vaddr);
}

/// Converts a kernel logical virtual address to a virtual page address
struct page* va_to_page(void* page_addr)
{
    if ((u32)page_addr & 0xFFF) {
        return NULL;
    }

    u32 page_index = ((u32)page_addr - KERNEL_START) / 4096;
    return page_array + page_index;
}

/// Converts a physical address to a virtual page address
struct page* pa_to_page(void* page_addr)
{
    u32 vaddr = (u32)_va(page_addr);
    if (vaddr & 0xFFF) {
        return NULL;
    }

    u32 page_index = (vaddr - KERNEL_START) / 4096;
    return page_array + page_index;
}

/// Initializes the page used for secondary level page tables
void lv2_pt_init(struct page* page)
{
    struct pt2* pt = (struct pt2 *)page_to_va(page);
    pt->bitmap = 0xFFFFFFFF;
}

/// Returns the virtual address of a L2 page table whitin the specified page
u32* lv2_pt_find_in_page(struct page* page)
{
    if (page == NULL) {
        return NULL;
    }
    struct pt2* pt = (struct pt2 *)page_to_va(page);
    for (u32 i = 0; i < 3; i++) {
        // Can only allocate when a bit is free
        if (pt->bitmap & (1 << i)) {   
            pt->bitmap |= (1 << i);
            return ((u32 *)pt + 0x100 * i);
        }
    }
    return NULL;
}

/// Initializes a mm_process structure
void mm_process_init(struct mm_process* mm)
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

/// Takes in the base virtual address of a 1M mapping and a level 2 page table,
/// and creates a new entry in the level 1 page table given by `ttbr_paddr` with
/// the given domain
void lv1_pt_map_in_lv2_pt(u32* ttbr_paddr, u32* pt2_vaddr, u32 vaddr, u8 domain)
{
    if (vaddr & 0xFFFFF) {
        panic("Unaligned page table mapping");
    }
    u32* pt1 = _va(ttbr_paddr);

    u32 pte = LV1_PT_PTR |
              LV1_PT_PTR_BASE((u32)_pa(pt2_vaddr)) |
              LV1_PT_PTR_DOMAIN(domain & 0xF);

    pt1[vaddr >> 20] = pte;
}

/// Returns if a lv1 page table entry is mapped. The entry must be in the L1
/// range
static u32 lv1_is_mapped(u32* ttbr_paddr, u32 entry)
{
    u32* vaddr = _va(ttbr_paddr);
    return !pte_is_empty(vaddr[entry]);
}

/// Maps in a new 4k page into the page table. This returns 1 if the region was
/// successfully mapped and zero if the l2 page table does not exist
u32 lv2_pt_map_page(u32* ttbr_paddr, u32* page_vaddr, u32 vaddr, u32 attr)
{
    if (vaddr & 0xFFF) {
        panic("Virtual page address is not aligned");
    }

    u32* pt1 = _va(ttbr_paddr);
    u32 pte = pt1[vaddr >> 20];

    if (pte_is_empty(pte)) {
        return 0;
    }

    u32* pt2 = _va((u32 *)LV1_PT_PTR_BASE(pte));

    pt2[(vaddr >> 12) & 0xFF] = LV2_PT_SECTION_BASE((u32)_pa(page_vaddr)) |
                                attr;
    return 1;
}

/// Maps in a number of pages for a user process, into the process mm structure
u32 mm_process_map_memory(struct mm_process* mm, struct page* page,
    u32 page_cnt, u32 vaddr, u32 attr, u8 domain)
{
    if (vaddr & 0xFFF) {
        panic("Non aligned");
    }
    
    while (page_cnt--) {
        // Map in page to the vaddr
        if (lv1_is_mapped(mm->ttbr_phys, vaddr >> 20) == 0) {
            u32* new_vaddr = lv2_pt_find_in_page(mm->pt2_ptr);

            if (!new_vaddr) {

                // We dont have any more secondary page tables
                struct page* new_page = lv2_pt_alloc();
                if (!new_page) {
                    return 0;
                }
                mm->pt2_ptr = new_page;
                mm_process_add_page(new_page, mm);
                lv2_pt_init(new_page);

                new_vaddr = lv2_pt_find_in_page(mm->pt2_ptr);

                if (!new_vaddr) {
                    panic("Panic");
                }
            }
            lv1_pt_map_in_lv2_pt(mm->ttbr_phys, new_vaddr, vaddr & ~(0xFFFFF),
                domain);
        }

        lv2_pt_map_page(mm->ttbr_phys, page_to_va(page), vaddr, attr);

        vaddr += 4096;
        page++;
    }
    return 1;
}

/// This functions sets the heap break point. This also remappes the heap 
/// pointers if 
u32* set_break(u32 bytes)
{
    print("SET BRAKE\n");
    struct mm_process* mm = get_curr_mm_process();

    if (mm->heap_e == 0) {
        // Heap is not mapped
        mm->heap_s = mm->data_e;

        // Align the addresses with a page
        align_up(mm->heap_s, 4096);
        mm->heap_e = mm->heap_s;
    }

    if (bytes == 0) {
        return mm->heap_e;
    }

    u32 pages = align_up_val(bytes, 4096) / 4096;
    u32 order = pages_to_order(pages);

    struct page* page_ptr = alloc_pages(order);

    if (!page_ptr) {
        return mm->heap_e;
    }
    print(GREEN "Number of pages allocated %d\n" NORMAL, 1 << order);
    curr_thread_add_pages(page_ptr, 1 << order);

    u32 flags = LV2_PT_SECTION |
                LV2_PT_SECTION_FULL_ACC |
                LV2_PT_SECTION_WRITE_THROUGH;

    u32 domain = 15;

    mm_process_map_memory(mm, page_ptr, 1 << order, (u32)mm->heap_e, flags, domain);

    // Extend the heap region
    mm->heap_e += 4096 * (1 << order);

    return mm->heap_e;
}
