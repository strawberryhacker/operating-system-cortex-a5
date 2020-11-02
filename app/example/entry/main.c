/// Copyright (C) strawberryhacker

#include <citrus.h>
#include <types.h>
#include <regmap.h>
#include <stddef.h>

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
    syscall_create_thread(thread, 500, "elf_child", NULL, 0);
    while (1) {
        print("Hello from ELF binary\n");
        syscall_thread_sleep(500000);
    }
}
