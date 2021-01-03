/// Copyright (C) strawberryhacker 

#ifndef PRINT_H
#define PRINT_H

#include <citrus/types.h>
#include <stdarg.h>

#define RED     "\033[31m"
#define NORMAL  "\033[0m"
#define GREEN   "\033[32m"
#define PINK    "\033[35m"
#define BLUE    "\033[34m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"

void print_init(void);
void print_dma_init(void);

void print(const char* data, ...);
void print_task(const char* data, ...);

void print_dma(const char* data, ...);

#endif
