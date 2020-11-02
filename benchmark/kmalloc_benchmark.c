/// Copyright (C) strawberryhacker 

#include <citrus/benchmark.h>
#include <citrus/kmalloc.h>
#include <citrus/print.h>
#include <stdlib.h>

struct page_bb {
    u8* ptr;
    u32 size;
    u8 fill;
};

static u32 used_size = 0;

#define BENCHMARK_SIZE 0x8000
#define MAX_SIZE 100000

#define MAX 10000000
#define MIN 400

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
        if (free && benchmark[rand_index].size == 0)
            return rand_index;
        if (!free && (benchmark[rand_index].size != 0))
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
        return 0;
    }

    struct page_bb* obj = &benchmark[free_index];

    
    for (u32 i = 0; i < obj->size; i++) {
        if (obj->ptr[i] != obj->fill) {
            print("error error\n");
            while (1);
        }
    }
    start_cycle_counter();
    kfree(obj->ptr);
    u32 cycles = get_cycles();

    used_size -= obj->size;

    obj->size = 0;

    print("F => cycles %7d used %10d kB size %d\n", cycles, used_size / 1000, obj->size);
    return 1;
}

static u32 _alloc(void)
{
    u32 size = 1 + rand() % MAX_SIZE;

    start_cycle_counter();
    void* ptr = kmalloc(size);
    u32 cycles = get_cycles();

    if (ptr == NULL) {
        return 0;
    }

    used_size += size;

    u32 new_index = get_free_used_index(1);
    
    u8 fill = rand() % 0xFF;

    for (u32 i = 0; i < size; i++) {
        ((u8 *)ptr)[i] = fill;
    }

    if (new_index < 0) {
        return 0;
    }

    benchmark[new_index].ptr = ptr;
    benchmark[new_index].size = size;
    benchmark[new_index].fill = fill;

    print("A => cycles %7d used %10d kB size %d\n", cycles, used_size / 1000, size);
    return 1;
}

void kmalloc_benchmark(void)
{
    for (u32 i = 0; i < BENCHMARK_SIZE; i++) {
        benchmark[i].ptr = NULL;
        benchmark[i].size = 0;
    }

    while (1) {
        while (1) {
            if (!_alloc()) break;
            if (used_size > MAX) {
                break;
            }
        }
        while (1) {
            if (!_free()) {
                break;
            }
            if (used_size < MIN) {
                break;
            }
        }
    }
    while (1);
}
