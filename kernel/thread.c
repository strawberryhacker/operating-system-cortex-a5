// Copyright (C) strawberryhacker

#include <citrus/thread.h>
#include <citrus/print.h>
#include <citrus/kmalloc.h>
#include <citrus/page_alloc.h>
#include <citrus/align.h>
#include <citrus/mm.h>
#include <citrus/pt_entry.h>
#include <citrus/process.h>
#include <citrus/cache.h>
#include <citrus/sched.h>
#include <citrus/atomic.h>
#include <citrus/panic.h>
#include <citrus/interrupt.h>
#include <citrus/syscall.h>
#include <citrus/mem.h>
#include <citrus/pid.h>

// Initial CPSR values for kernel / user threads. The mode is set, interrupts
// are unmasked and the status bits are cleared
#define USER_THREAD_CPSR  0b10000
#define KERNEL_THREAD_CPSR 0b11111

// The scheduling class occupies the three lowest bits in the thread attributes
#define FLAG_CLASS_MSK 0b111
#define FLAG_CLASS_POS 0
    
volatile u8 p = 0;

// Thread and process exit routine which is called when either a thread or a 
// process exits
void thread_exit(u32 status_code)
{
    u32 flags = __atomic_enter();
    struct thread* t = get_curr_thread();
    __atomic_leave(flags);

    // TODO kill the thread
    print("Quitting  PID: %d with status %d\n", t->pid, status_code);

    while (1) {
        syscall_thread_sleep(500);
    }

    // Delete the thread
    if (t->mmap) {
        if (t->process == t) {
            print("Killing a process\n");
        } else {
            print("Killing a thread\n");

            // Remove the thread from any lists

            kfree(t);
        }
    } else {
        // We will kill a kernel thread
    }

    while (1) {
        //syscall_thread_sleep(1);
    }

    // REMEMBER TO CHANGE THE RQ->LAZY_FPU POINTER IF THE THREAD HAS A LAZY
    // CONTEXT STILL IN THE CORE
    //
    // The rq->lazy_fpu should be set to NULL when a thread is deleted.
}

// Sets up the stack for any process. This takes in the arguments and the
// return function as well as the CPSR for user / kernel threads
u32* stack_setup(u32* sp, i32 (*func)(void *), void* args, u32 cpsr)
{
    sp--;
    *sp-- = cpsr;              // cpsr  
    *sp-- = (u32)func;         // pc    
    *sp-- = 0x12121212;        // r12   
    *sp-- = 0x03030303;        // r3    
    *sp-- = 0x02020202;        // r2    
    *sp-- = 0x01010101;        // r1    
    *sp-- = (u32)args;         // args  

    *sp-- = (u32)thread_exit;  // r12 - LR

    // To optimize the context switch time the context switch is closly embedded
    // into the IRQ routine. The IRQ padds the stack pointer in order to
    // archeive 8-byte alignment. This is because the IRQ routine calls
    // C-functions and has to be AAPCS compliant
    *sp-- = 0;  

    *sp-- = 0x11111111;        // r11   
    *sp-- = 0x10101010;        // r10   
    *sp-- = 0x09090909;        // r9    
    *sp-- = 0x08080808;        // r8    
    *sp-- = 0x07070707;        // r7    
    *sp-- = 0x06060606;        // r6    
    *sp-- = 0x05050505;        // r5    
    *sp   = 0x04040404;        // r4

    return sp;
}

// Initializes the thread structure. The thread structure MUST be allocated
// using kzalloc. This way the mm field will be zero which is required for
// kernel threads whithin the context switch
static void init_thread_struct(struct thread* thread)
{
    // Initialize the list nodes
    list_node_init(&thread->node);
    list_node_init(&thread->thread_group);
    list_node_init(&thread->thread_node);

    // Initialize the FPU context - 32 registers
    mem_set(thread->fpu_stack, 0, 32 * 4);
}

// Copies in the thread name into the thread control block. This adds NULL
// termination such that the thread name can be easily printed
static void thread_set_name(struct thread* thread, const char* name)
{
    u32 i;
    char* dest = thread->name;

    for (i = 0; i < THREAD_MAX_NAME - 1 && *name; i++) {
        *dest++ = *name++;
    }
    *dest = '\0';
}

// Sets the scheduler class in a thread given the thread flags
static void thread_set_sched_class(struct thread* thread, u32 flags)
{
    const struct sched_class* class = get_sched_class(flags & 0b111);
    thread->class = class;
}

// Creates a lightweight kernel thread in the kernel memory space
struct thread* create_kthread(i32 (*func)(void *), u32 stack_words, 
    const char* name, void* args, u32 flags)
{
    // Allocate a new thread struct + stack in the same allocation
    u32 alloc_size = sizeof(struct thread) + stack_words * 4;
    alloc_size = align_up(alloc_size, 8);
    struct thread* thread = kzmalloc(alloc_size);

    init_thread_struct(thread);    

    // Set the name of the thread
    thread_set_name(thread, name);

    // Assign a new PID
    i32 err = alloc_pid(&thread->pid);
    if (err < 0)
        return NULL;
    
    // The stack goes after the thread control block and will be 8 byte aligned
    thread->stack_base = (u32 *)((u8 *)thread + sizeof(struct thread));
    thread->stack_base = align_up_ptr(thread->stack_base, 8);

    // Update the stack pointer address
    thread->stack = thread->stack_base + stack_words - 1;
    thread->stack = stack_setup(thread->stack, func, args, KERNEL_THREAD_CPSR);

    // Add the thread to the global thread list
    sched_add_thread(thread);
    thread_set_sched_class(thread, flags);
    sched_enqueue_thread(thread);

    dcache_clean();
    icache_invalidate();

    return thread;
}

// TODO implement the kill kernel thread function
void kill_kernel_thread(struct thread* t)
{

}

// Core function for creating a user thread. This assumes that a memory space 
// is created. It will allocate a new stack region
static inline void create_user_thread_core(struct thread* thread,
    i32 (*func)(void *), u32 stack_words,const char* name, void* args, u32 flags)
{
    assert(thread->mmap != NULL);

    // Set the name of the thread
    thread_set_name(thread, name);

    // Map in the stack region. This will allocate a number of pages and map
    // them into the stack region in the process memory map
    u32 stack_page_cnt = (u32)align_up_ptr((void *)stack_words, 4096) / 4096;
    u32 stack_order = pages_to_order(stack_page_cnt);
    stack_page_cnt = (1 << stack_order);
    
    // Allocate and add the pages
    struct page* stack_page_ptr = alloc_pages(stack_order);
    mm_process_add_page(stack_page_ptr, thread->mmap);

    struct pte_attr attr = {
        .access = PTE_ACCESS_FULL_ACC,
        .mem    = PTE_MEM_WRITE_THROUGH,
        .domain = 15,
        .nG     = 0,
        .xn     = 0
    };

    // Allocate space in the process virtual memory space for the stack
    thread->mmap->stack_e -= stack_page_cnt * (4096 / 4);

    // Map in the stack in the process virtual memory
    u8 status = mm_map_in_pages(thread->mmap, stack_page_ptr, stack_page_cnt, 
        (u32)thread->mmap->stack_e, &attr);

    assert(status);

    // Invalidate the stack region in the D-cache
    u32 start = (u32)page_to_va(stack_page_ptr);

    // Setup the stack using kernel logical addressing
    u32* sp_kern_virt = page_to_va(stack_page_ptr);
    u32* sp_usr_virt = thread->mmap->stack_e;

    u32* sp = sp_kern_virt + stack_page_cnt * 1024 - 1;
    sp = stack_setup(sp, func, args, USER_THREAD_CPSR);
    thread->stack = sp_usr_virt + (sp - sp_kern_virt);

    dcache_clean();
    icache_invalidate();

    // Add the thread to the global thread list
    sched_add_thread(thread);
    thread_set_sched_class(thread, flags);
    sched_enqueue_thread(thread);
}

// Creates a user thread within the memory space of the parent process
struct thread* create_thread(i32 (*func)(void *), u32 stack_words, 
    const char* name, void* args, u32 flags)
{
    struct thread* thread = kzmalloc(sizeof(struct thread));
    //print("Creating a user thread: %p\n", thread);

    init_thread_struct(thread);

    // Find the parent thread
    struct thread* parent = get_curr_thread();

    // Bind the new thread to the current running process
    thread->mmap = parent->mmap;
    thread->process = parent;
    list_add_first(&thread->thread_group, &parent->thread_group);

    // Create the thread
    create_user_thread_core(thread, func, stack_words, name, args, flags);

    dcache_clean();
    icache_invalidate();

    return NULL;
}

// Create new heavy process
struct thread* create_process(i32 (*func)(void *), u32 stack_words,
    const char* name, void* args, u32 flags)
{
    struct thread* thread = kzmalloc(sizeof(struct thread));
    //print("Creating a user process: %p\n", thread);

    init_thread_struct(thread);

    // Make a new memory space
    process_mm_init(thread, stack_words);
    create_user_thread_core(thread, func, stack_words, name, args, flags);

    // Must be initialized after the create_thread_core beacuse it initializes
    // the thread group as a list node
    thread->process = thread;
    list_init(&thread->thread_group);

    dcache_clean();
    icache_invalidate();

    return thread;
}

// This functions maps in a number of code pages into the process memory space
void map_in_code(struct page* code_page, u32 pages, struct thread* thread)
{
    thread->mmap->data_s += pages * 4096;

    struct pte_attr attr = {
        .access = PTE_ACCESS_FULL_ACC,
        .mem    = PTE_MEM_WRITE_THROUGH,
        .domain = 15,
        .nG     = 0,
        .xn     = 0
    };

    // Map in the memory
    u32 status = mm_map_in_pages(thread->mmap, code_page, pages, 0x00100000, &attr);
    assert(status);

    mm_tlb_invalidate();
    dcache_clean();
    icache_invalidate();
}

// Adding a number of pages to a thread. This also updated the parent process
// memory makp, and adds the pages to the mm pages list
void curr_thread_add_pages(struct page* page, u32 pages)
{
    struct thread* t = get_curr_thread();

    t->page_cnt += pages;
    t->mmap->page_cnt += pages;

    mm_process_add_page(page, t->mmap);
}

// Returns the memory managment structure of the current (parent) process of 
// the current running thread
struct mmap* get_curr_mm_process(void)
{
    struct thread* t = get_curr_thread();
    return t->mmap;
}
