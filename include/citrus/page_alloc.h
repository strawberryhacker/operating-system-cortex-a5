/// Copyright (C) strawberryhacker 

#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include <citrus/types.h>

struct page;

struct page* alloc_page(void);
struct page* alloc_pages(u32 order);
void free_pages(struct page* page);

u32 bytes_to_order(u32 bytes);
u32 pages_to_order(u32 pages);

// Allocating a 8k user page table and retruns the kernel physical address 
struct page* alloc_lv1_page_table(void);
struct page* alloc_lv2_page_table(void);

void free_lv1_page_table(struct page* page);
void free_lv2_page_table(struct page* page);

/// These functions converts between a kernel logical struct page and wither a 
/// physical or virtual page address
void* page_to_va(struct page* page);
void* page_to_pa(struct page* page);

struct page* va_to_page(void* page_addr);
struct page* pa_to_page(void* page_addr);

#endif
