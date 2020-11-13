/// Copyright (C) strawberryhacker 

#ifndef MATRIX_H
#define MATRIX_H

#include <citrus/types.h>
#include <citrus/regmap.h>

struct matrix_reg* get_bus(u8 pid);
u8 is_secure(struct matrix_reg* matrix, u8 pid);
void set_non_secure(u8 pid);
void set_secure(u8 pid);

#endif
