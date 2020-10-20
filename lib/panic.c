/// Copyright (C) strawberryhacker

#include <cinnamon/panic.h>
#include <cinnamon/print.h>
#include <regmap.h>

void panic_handler(const char* file, u32 line, const char* reason)
{
    print("Panic! at line %d in file ", line);
    print(file);
    print("\nReason: ");
    print(reason);
    print("\n");

    // Flush the serial buffer
    while (!(UART1->SR & (1 << 9)));
    RST->CR = 0xA5000000 | 1;
}
