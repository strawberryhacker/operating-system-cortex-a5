/// Copyright (C) strawberryhacker 

#include <citrus/benchmark.h>
#include <citrus/page_alloc.h>
#include <citrus/print.h>
#include <citrus/panic.h>
#include <citrus/mm.h>
#include <stdlib.h>

struct page_bb {
    struct page* page;
    i32 order;
};

static u32 used_size = 0;

#define BENCHMARK_SIZE 0x8000
#define MAX_ORDER 9

#define MAX (30 * BENCHMARK_SIZE / 100)
#define MIN 0

static struct page_bb benchmark[BENCHMARK_SIZE];

static inline void start_cycle_counter(void)
{
    u32 reg;
    asm volatile ("mrc p15, 0, %0, c9, c12, 1" : "=r" (reg));
    reg |= (1 << 31);
    asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (reg));

    asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (reg));
    reg |= (1 << 0) | (1 << 2);
    asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (reg));
}

static inline u32 get_cycles(void)
{
    u32 cycles;
    asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (cycles));

    return cycles;
}

static i32 get_free_used_index(u32 free)
{
    u32 rand_index = rand() % BENCHMARK_SIZE;
    u32 cnt = BENCHMARK_SIZE;
    while (cnt--) {
        if (free && benchmark[rand_index].order == -1)
            return rand_index;
        if (!free && (benchmark[rand_index].order >= 0))
            return rand_index;

        if (++rand_index >= BENCHMARK_SIZE) {
            rand_index = 0;
        }
    }
    return -1;
}

static u32 _free(void)
{
    u32 free_index = get_free_used_index(0);

    if (free_index < 0) {
        panic("Error\n");
        return 0;
    }

    struct page_bb* free_page = &benchmark[free_index];

    start_cycle_counter();
    if (free_page->order != free_page->page->order){
        panic("OK\n");
    }
    free_pages(free_page->page);
    u32 cycles = get_cycles();

    used_size -= (1 << free_page->order);

    free_page->order = -1;
    print("F => cycles %5d used %5d size %d\n", cycles, used_size, (1 << free_page->order));
    return 1;
}

static u32 _alloc(void)
{
    u32 rand_order = rand() % MAX_ORDER;

    start_cycle_counter();
    struct page* new_page = alloc_pages(rand_order);
    u32 cycles = get_cycles();

    if (new_page == NULL) {
        panic("Error\n");
        return 0;
    }

    used_size += (1 << rand_order);

    u32 new_index = get_free_used_index(1);
    u32 tag = rand() % 0xFFFFFFFF;

    if (new_index < 0) {
        panic("Error\n");
        return 0;
    }

    benchmark[new_index].page = new_page;
    benchmark[new_index].order = rand_order;

    print("A => cycles %5d used %5d size %d\n", cycles, used_size, (1 << rand_order));
    return 1;
}

void page_alloc_benchmark(void)
{
    struct page* new_page = alloc_pages(0);

    srand(5);

    for (u32 i = 0; i < BENCHMARK_SIZE; i++) {
        benchmark[i].page = NULL;
        benchmark[i].order = -1;
    }

    while (1) {
        while (1) {
            if (!_alloc()) break;
            if (used_size > MAX) {
                break;
            }
        }

        print("Total memory allocated => %d\n", used_size);
        while (1) {
            if (!_free()) break;
            if (used_size <= MIN) {
                break;
            }
        }
    } 
}
