/// Copyright (C) strawberryhacker 

#ifndef SPRINT_H
#define SPRINT_H

#include <citrus/types.h>
#include <stdarg.h>

u32 vsprint(char* buf, const char* data, va_list args);

u32 sprint(char* buf, const char* data, ...);

#endif
