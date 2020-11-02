/// Copyright (C) strawberryhacker

#ifndef STRING_H
#define STRING_H

#include <citrus/types.h>

void string_add_name(char* dest, const char* name, u32 size);

u32 string_length(const char* string);

void string_copy(const char* src, char* dest);

#endif
