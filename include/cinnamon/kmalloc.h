/// Copyright (C) strawberryhacker 

#ifndef KMALLOC_H
#define KMALLOC_H

#include <cinnamon/types.h>

void* kmalloc(u32 size);
void* kzmalloc(u32 size);
void kfree(void* ptr);

#endif
