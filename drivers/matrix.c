// Copyright (C) strawberryhacker 

#include <citrus/matrix.h>

// Defines the PID numbers of the peripherals connected to the H64MX matrix 
static const u8 h64mx_table[] =
    { 2, 6, 7, 9, 10, 12, 13, 15, 31, 32, 45, 46, 52, 53, 63 };

// Any peripheral is either connnected to the H32MX bus at max 83MHz or the
// H64MX bus at max 166 MHz. This function returns the register pointer to the
// bus the peripheral is connected to
struct matrix_reg* get_bus(u8 pid)
{
    for (u8 i = 0; i < sizeof(h64mx_table); i++) {
        if (pid == h64mx_table[i]) {
            return H64MX;
        }
    }
    return H32MX;
}

// Get the security configuration of a peripheral
u8 is_secure(struct matrix_reg* matrix, u8 pid)
{
     const u32* reg = (const u32 *)&matrix->SPSELR1;
     if (reg[pid / 32] & (1 << (pid % 32))) {
         return 0;
     } else {
         return 1;
     }
}

void set_non_secure(u8 pid)
{
    struct matrix_reg* matrix = get_bus(pid);
    u32* reg = (u32 *)&matrix->SPSELR1;
    reg[pid / 32] &= ~(1 << (pid % 32));
}

void set_secure(u8 pid)
{
    struct matrix_reg* matrix = get_bus(pid);
    u32* reg = (u32 *)&matrix->SPSELR1;
    reg[pid / 32] |= (1 << (pid % 32));
}
