// Copyright (C) strawberryhacker 

#include <citrus/sprint.h>
#include <citrus/regmap.h>
#include <stddef.h>

// Simple sprintf implementation. Floating point not supported yet. For more 
// information on how to use sprintf; have a look here:
// https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm

static inline u8 char_to_num(char c)
{
    return (u8)(c - '0');
}

static inline u8 is_num(char c)
{
    return ((c >= '0') && (c <= '9')) ? 1 : 0;
}

// Flags definitions 
#define FLAG_LEFT    0x01
#define FLAG_PLUS    0x02
#define FLAG_SPACE   0x04
#define FLAG_ZERO    0x08
#define FLAG_SPECIAL 0x10
#define FLAG_SMALL   0x20
#define FLAG_SIGN    0x40

// Look up table for capital letters. To get the lowercase or with 0x20 
static const char letter_lookup[16] = "0123456789ABCDEF";

static inline char* put_number(char* buf, i32 num, u8 flags, u8 base, u8 width)
{
    char pad = (flags & FLAG_ZERO) ? '0' : ' ';
    u8 lower = (flags & FLAG_SMALL) ? FLAG_SMALL : 0;
    char sign = 0;

    char tmp[32];

    // Get the sign - 0 for no sign 
    u32 u_num = (u32)num;
    if (flags & FLAG_SIGN) {
        if (num < 0) {
            u_num = (u32)-num;
            sign = '-';
        } else if (flags & FLAG_PLUS) {
            sign = '+';
        } else if (flags & FLAG_SPACE) {
            sign = ' ';
        }
    }

    // Print the number to the tmp buffer 
    u8 i = 0;
    if (u_num == 0) {
        tmp[i++] = '0';
    } else while (u_num) {
        tmp[i++] = letter_lookup[u_num % base] | lower;
        u_num /= base;
    }

    if (flags & FLAG_SPECIAL) {
        *buf++ = '0';
        if (base == 2) {
            *buf++ = 'b';
        } else if (base == 16){
            *buf++ = 'x';
        }
    }
    if (sign) {
        tmp[i++] = sign;
    }

    if (i > width) {
        width = i;
    }

    u8 pad_size = width - i;
    if (!(flags & FLAG_LEFT)) {
        while (pad_size--) {
            *buf++ = pad;
        }
    }
    // At least one character is written 
    do {
        *buf++ = tmp[--i];
    } while (i);

    if (flags & FLAG_LEFT) {
        while (pad_size--) {
            *buf++ = pad;
        }
    }
    return buf;
}

u32 vsprint(char* buf, const char* data, va_list args)
{
    // NULL check
    if (!data) {
        return 0;
    }
    char* start = buf;
    for (; *data; ++data) {
        if (*data != '%') {
            *buf++ = *data;
            continue;
        }

        // Process flags 
        u8 flags = 0;
        flags:
            data++;
            switch(*data) {
                case '0': flags |= FLAG_ZERO;    goto flags;
                case '-': flags |= FLAG_LEFT;    goto flags;
                case ' ': flags |= FLAG_SPACE;   goto flags;
                case '#': flags |= FLAG_SPECIAL; goto flags;
                case '+': flags |= FLAG_PLUS;    goto flags;
            }

        // Get the format number 
        i32 width = 0;
        if (*data == '*') {
            width = (i32)va_arg(args, int);
            data++;
        } else {
            while (is_num(*data)) {
                width = (width * 10) + char_to_num(*data++);
            }
        }

        const char* src;
        u8 base = 10;
        switch (*data) {
        case 's':
            src = va_arg(args, char *);
            if (!src) {
                src = "<NULL>";
                width = 6;
            }
            if (width) {
                u32 name = 0;
                for (name = 0; name < width; name++) {
                    if (src[name] == '\0') {
                        break;
                    }
                }
                u32 padd = width - name;

                if ((flags & FLAG_LEFT) == 0) {
                    for (u32 i = 0; i < padd; i++) {
                        *buf++ = ' ';
                    }
                }
                for (u32 i = 0; i < name; i++) {
                    *buf++ = *src++;
                }
                if (flags & FLAG_LEFT) {
                    for (u32 i = 0; i < padd; i++) {
                        *buf++ = ' ';
                    }
                }
            } else {
                while (*src) {
                    *buf++ = *src++;
                }
            }
            continue;
        case 'x':
            flags |= FLAG_SMALL;
        case 'X':
            base = 16;
            break;
        case 'd':
        case 'i':
            flags |= FLAG_SIGN;
        case 'u':
            break;
        case 'p':
            flags |= FLAG_SPECIAL;
            flags |= FLAG_SMALL;
            flags |= FLAG_ZERO;
        case 'P':
            base = 16;
            width = 8;
            break;
        case 'b':
            flags |= FLAG_SMALL;
        case 'B':
            base = 2;
            break;
        case '%':
            *buf++ = '%';
            continue;
        case 'c':
            *buf++ = (char)va_arg(args, int);
            continue;
        default:
            // Adjust for the ++data in the for loop 
            --data;
            continue;
        }

        // Print the number 
        i32 num = (i32)va_arg(args, long);
        buf = put_number(buf, num, flags, base, (u8)width);
    }
    return buf - start;
}

// General sprintf implentation
u32 sprint(char* buf, const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buf, data, args);
    va_end(args);

    return cnt;
}
