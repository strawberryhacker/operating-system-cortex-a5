// Copyright (C) strawberryhacker

#include <citrus/pid.h>
#include <citrus/kmalloc.h>
#include <citrus/print.h>
#include <citrus/error.h>
#include <citrus/panic.h>
#include <citrus/mem.h>
#include <citrus/thread.h>

// This file implements a recursive binary PID tree for fast assignment of new
// PIDs and fast freeing of exsisting PIDs

#define PID_LEVELS 2

// Base pointer for the pid table
static u32* pid_table;

void pid_init(void)
{
    // Geometric series computes the allocation size of recursive bitmap
    u32 pid_size = (__builtin_pow(32, PID_LEVELS) - 1) / 31;

    // We use kmalloc for allocating the PID bitmap during boot
    pid_table = kmalloc(4 * pid_size);

    // Set all the bis - marking all the PIDs as free
    mem_set(pid_table, 0xFF, pid_size * 4);
}

// Allocated a PID number using the recursice bitmap PID allocator. If updates 
// the value at the given address
//
// Returns 0 if a new valid PID is returned and -ENOPID if the allocation failed
i32 alloc_pid(pid_t* pid)
{
    u32 pow = __builtin_pow(32, PID_LEVELS - 1);

    u8 bit;
    u32 new_pid = 0;
    u32 curr_index = 0;

    // Iterate the bitmap and find the first free PID
    for (u32 i = 0; i < PID_LEVELS; i++) {
        bit = __builtin_ctz(pid_table[curr_index]);

        // If the first level is all zeros the bitmap is full
        if (bit == 32 && i == 0)
            return -ENOPID;
        
        // Compute the child index
        if (i < (PID_LEVELS - 1))
            curr_index = 1 + (curr_index << 5) + bit;

        new_pid += pow * bit;
        pow >>= 5;
    }

    // The last order if set in any case
    pid_table[curr_index] &= ~(1 << bit);

    // Last level has only used bits. We have to recursivly update the entire
    // bitmap
    if (bit == 31) {
        for (u32 i = 0; i < (PID_LEVELS - 1); i++) {
            bit = (curr_index  - 1) & 0xFFFFF;
            curr_index = (curr_index  - 1) >> 5;

            // Clear the bit in the previous register
            pid_table[curr_index] &= ~(1 << bit);

            // Check if we have to continue recusion
            if (__builtin_ctz(pid_table[curr_index]) < 32) {
                break;
            }
        }
    }

    *pid = new_pid;
    return 0;
}

// Frees the specified PID number
void free_pid(pid_t pid)
{
    u32 index = (__builtin_pow(32, PID_LEVELS - 1) - 1) / 31;

    index += pid >> 5;
    u32 bit = pid & 0xFFFFF;

    // This checks if the tried freed PID acctually has been allocated
    if (pid_table[index] & (1 << bit))
        panic("PID allocator failure");

    for (u32 i = 0; i < PID_LEVELS; i++) {

        u32 reg = pid_table[index];

        // Mark bit as free
        pid_table[index] |= (1 << bit);

        // If the bitmap is non-empty the parent bit will not change
        if (reg)
            return;

        bit = (index  - 1) & 0xFFFFF;
        index = (index  - 1) >> 5;
    }
}
