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

u32 entry(void* args)
{
    while (1) {
        print("Hello\n");
        syscall_thread_sleep(1000);
    }
    return 5;
}
