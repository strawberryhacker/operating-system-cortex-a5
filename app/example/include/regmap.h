/// Copyright (C) strawberryhacker

#ifndef REGMAP_H
#define REGMAP_H

// Types used for registers 
#define __r volatile const
#define __w volatile 
#define _rw volatile

// UART registers 
struct uart_reg {
    __w u32 CR;
    _rw u32 MR;  
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    __r u32 SR;
    __r u32 RHR;
    __w u32 THR;
    _rw u32 BRGR;
    _rw u32 CMPR;
    _rw u32 RTOR;
    __r u32 RESERVED[46];
    _rw u32 WPMR;
};

#define UART0 ((struct uart_reg *)0xF801C000)
#define UART1 ((struct uart_reg *)0xF8020000)
#define UART2 ((struct uart_reg *)0xF8024000)
#define UART3 ((struct uart_reg *)0xFC008000)
#define UART4 ((struct uart_reg *)0xFC00C000)

#endif
