/// Copyright (C) strawberryhacker

#ifndef PANIC_H
#define PANIC_H

#include <citrus/types.h>

#define PANIC_ENABLE 1
#define WARNING_ENABLE 1
#define ASSERT_ENABLE 1

#if PANIC_ENABLE
#define panic(reason) panic_handler(__FILE__, __LINE__, (reason))
#else
#define panic(reason)
#endif

#if ASSERT_ENABLE
#define assert(statement) assert_handler(__FILE__, __LINE__, ((statement) != 0));
#else
#define assert(statement)
#endif

#if WARNING_ENABLE
#define warning(reason) warning_handler(__FILE__, __LINE__, (reason))
#else
#define warning(reason)
#endif

void panic_handler(const char* file, u32 line, const char* reason);

void warning_handler(const char* file, u32 line, const char* reason);

void assert_handler(const char* file, u32 line, u32 statement);

#endif
