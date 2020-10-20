/// Copyright (C) strawberryhacker

#include <cinnamon/thread.h>
#include <cinnamon/print.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/page_alloc.h>
#include <cinnamon/align.h>
#include <cinnamon/mm.h>
#include <cinnamon/pt_entry.h>
#include <cinnamon/process.h>
#include <cinnamon/cache.h>
#include <cinnamon/sched.h>
#include <cinnamon/atomic.h>
#include <cinnamon/panic.h>
#include <cinnamon/interrupt.h>

#define THREAD_CPSR 0b10000

#define FLAG_CLASS_MSK 0b111
#define FLAG_CLASS_POS 0

/// 
void thread_exit(void)
{
    irq_disable();
    u32 flags = __atomic_enter();
    struct thread* t = get_curr_thread();
    __atomic_leave(flags);

    print("Quitting thread with PID => %d\n", t->pid);
    while (1);
}

u32* stack_setup(u32* sp, void (*func)(void *), void* args)
{
    sp--;
    *sp-- = THREAD_CPSR;       // cpsr  
    *sp-- = (u32)func;         // pc    
    *sp-- = 0x12121212;        // r12   
    *sp-- = 0x03030303;        // r3    
    *sp-- = 0x02020202;        // r2    
    *sp-- = 0x01010101;        // r1    
    *sp-- = (u32)args;         // args  

    *sp-- = (u32)thread_exit;  // r12   

    // To optimize the context switch time the context switch is closly embedded
    // into the IRQ routine. The IRQ padds the stack pointer  in order to
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

static void init_thread_struct(struct thread* thread)
{
    thread->page_cnt = 0;

    // Initialize the list nodes
    list_node_init(&thread->node);
    list_node_init(&thread->thread_group);
    list_node_init(&thread->thread_node);

    thread->tick_to_wake = 0;
}

static void thread_set_name(struct thread* thread, const char* name)
{
    u32 i;
    char* dest = thread->name;

    for (i = 0; i < THREAD_MAX_NAME - 1 && *name; i++) {
        *dest++ = *name++;

    }
    *dest = '\0';
}

static inline void create_thread_core(struct thread* thread, void (*func)(void *),
    u32 stack_size,const char* name, void* args, u32 flags)
{
    if (thread->mm == NULL) {
        panic("Need to setup memory space first");
    }

    // Set the name of the thread
    thread_set_name(thread, name);

    // Map in the stack region
    u32 stack_order = 
        pages_to_order((u32)align_up((void *)stack_size, 0x1000) / 0x1000);
    struct page* stack_page_ptr = alloc_pages(stack_order);
    mm_process_add_page(stack_page_ptr, thread->mm);

    u32 pt_flags = LV2_PT_SECTION |
                   LV2_PT_SECTION_FULL_ACC |
                   LV2_PT_SECTION_WRITE_THROUGH;
    u32 domain = 15;

    u32 stack_pages = (1 << stack_order);
    thread->mm->stack_e -= stack_pages * 0x400;

    mm_process_map_memory(thread->mm, stack_page_ptr, stack_pages, 
        (u32)thread->mm->stack_e, pt_flags, domain);

    // Invalidate the stack region in the D-cache
    u32 start = (u32)page_to_va(stack_page_ptr);
    dcache_clean_invalidate_range(start, start + stack_pages * 0x1000);

    // Set the stack using kernel logical addressing
    u32* sp_kern_virt = page_to_va(stack_page_ptr);
    u32* sp_usr_virt = thread->mm->stack_e;

    u32* sp = sp_kern_virt + stack_pages * 0x400 - 1;
    sp = stack_setup(sp, func, args);
    thread->sp = sp_usr_virt + (sp - sp_kern_virt);    

    // Add the thread to the global thread list
    sched_add_thread(thread);

    // Enqueue the process into the right scheduler
    const struct sched_class* class = get_sched_class(flags & 0b111);
    thread->class = class;
    
    // Queue the thread
    sched_enqueue_thread(thread);
}

/// Creating a thread
struct thread* create_thread(void (*func)(void *), u32 stack_size, 
    const char* name, void* args, u32 flags)
{
    print("Creating thread\n");

    struct thread* new = (struct thread *)kmalloc(sizeof(struct thread));
    init_thread_struct(new);

    // Find the parent thread
    struct thread* parent = get_curr_thread();

    // Binding the new thread to the current running process (parent)
    new->mm = parent->mm;
    new->process = parent;
    list_add_first(&new->thread_group, &parent->thread_group);
    print("ok\n");
    create_thread_core(new, func, stack_size, name, args, flags);

    dcache_clean();

    return NULL;
}

/// Creating a process
struct thread* create_process(void (*func)(void *), u32 stack_size,
    const char* name, void* args, u32 flags)
{
    print("Creating process\n");

    struct thread* new = (struct thread *)kmalloc(sizeof(struct thread));
    init_thread_struct(new);

    // Make a new memory space
    process_mm_init(new, stack_size);
    create_thread_core(new, func, stack_size, name, args, flags);

    // Must be initialized after the create_thread_core beacuse it initializes
    // the thread group as a list node
    new->process = new;
    list_init(&new->thread_group);

    dcache_clean();

    return new;
}

void map_in_code(struct page* code_page, u32 pages, struct thread* t)
{
    t->mm->data_s += pages * 0x1000;

    u32 flags = LV2_PT_SECTION |
                LV2_PT_SECTION_FULL_ACC |
                LV2_PT_SECTION_WRITE_THROUGH;

    u32 domain = 15;
    mm_process_map_memory(t->mm, code_page, pages, 0x00100000, flags, domain);
    mm_tlb_invalidate();
    icache_invalidate();
    dcache_clean();
}

/// Adding a number of pages to a thread. This also updated the parent process
/// memory makp, and adds the pages to the mm pages list
void curr_thread_add_pages(struct page* page, u32 pages)
{
    struct thread* t = get_curr_thread();

    t->page_cnt += pages;
    t->mm->page_cnt += pages;

    mm_process_add_page(page, t->mm);
}

/// Returns the memory managment structure of the current (parent) process of 
/// the current running thread
struct mm_process* get_curr_mm_process(void)
{
    struct thread* t = get_curr_thread();
    return t->mm;
}
