/// Copyright (C) strawberryhacker

#include <types.h>
#include <regmap.h>
#include <stddef.h>

#define SVC_ATTR __attribute__((naked)) __attribute__((noinline))

#define __SVC(x)        \
    asm volatile (      \
        "svc #"#x"\t\n" \
        "bx lr\n\t");

static void SVC_ATTR syscall_thread_sleep(u32 us)
{
    __SVC(8);
}

static u32 SVC_ATTR syscall_create_thread(void (*func)(void *),
    u32 stack_size, const char* name, void* args, u32 flags)
{
    __SVC(0);
}

void print(const char* data)
{
    while (*data) {
        while (!(UART1->SR & (1 << 1)));
        UART1->THR = *data++;
    }
}

void thread(void *args)
{
    while (1) {
        print("ELF child thread\n");
        syscall_thread_sleep(1000000);
    }
}

u32 entry(void* args)
{
    syscall_create_thread(thread, 500, "ksdf", NULL, 0);
    while (1) {
        print("Hello from ELF binary\n");
        syscall_thread_sleep(500000);
    }
}
