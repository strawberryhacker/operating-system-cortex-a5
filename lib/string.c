/// Copyright (C) strawberryhacker

#include <cinnamon/string.h>

/// This will copy name to the destimation buffer. The size specifies the 
/// dest buffer size, and this functions will add the NULL character at the end
void string_add_name(char* dest, const char* name, u32 size)
{    
    for (u32 i = 0; (i < size - 1) && *name; i++) {
        *dest++ = *name++;
    }
    *dest = '\0';
}
