// Copyright (C) strawberryhacker

#include <citrus/string.h>

// This will copy name to the destimation buffer. The size specifies the 
// dest buffer size, and this functions will add the NULL character at the end
void string_add_name(char* dest, const char* name, u32 size)
{    
    for (u32 i = 0; (i < size - 1) && *name; i++) {
        *dest++ = *name++;
    }
    *dest = '\0';
}

u32 string_length(const char* string)
{
    u32 cnt = 0;
    const char* start = string;
    while (*string) {
        string++;
    }
    return string - start;
}

// Copies the string pointed to by src to dest. The src string should be NULL
// terminated
void string_copy(const char* src, char* dest)
{
    while (*src) {
        *dest++ = *src++;
    }
    // NULL termination
    *dest = *src;
}
