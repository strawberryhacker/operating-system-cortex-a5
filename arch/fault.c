// Copyright (C) strawberryhacker 

#include <citrus/print.h>
#include <citrus/types.h>
#include <citrus/sched.h>
#include <citrus/panic.h>
#include <citrus/regmap.h>

// Performs a software reboot. This is called to avoid a manual hardware reset
// when the CPU does not respond to the c-boot interrupt instruction
void reboot(void)
{
    // Flush the serial buffer 
    print("Rebooting...\n");
    while (!(UART1->SR & (1 << 9)));
    RST->CR = 0xA5000000 | 1;
}

void print_regs(void)
{
    u32 reg;
    asm volatile ("mov %0, r0" : "=r" (reg));
    print("r0 : %p\n", reg);

    asm volatile ("mov %0, r1" : "=r" (reg));
    print("r1 : %p\n", reg);

    asm volatile ("mov %0, r2" : "=r" (reg));
    print("r2 : %p\n", reg);

    asm volatile ("mov %0, r3" : "=r" (reg));
    print("r3 : %p\n", reg);

    asm volatile ("mov %0, r4" : "=r" (reg));
    print("r4 : %p\n", reg);

    asm volatile ("mov %0, r5" : "=r" (reg));
    print("r5 : %p\n", reg);

    asm volatile ("mov %0, r6" : "=r" (reg));
    print("r6 : %p\n", reg);

    asm volatile ("mov %0, r7" : "=r" (reg));
    print("r7 : %p\n", reg);

    asm volatile ("mov %0, r8" : "=r" (reg));
    print("r8 : %p\n", reg);

    asm volatile ("mov %0, r9" : "=r" (reg));
    print("r9 : %p\n", reg);

    asm volatile ("mov %0, r10" : "=r" (reg));
    print("r10: %p\n", reg);

    asm volatile ("mov %0, r11" : "=r" (reg));
    print("r11: %p\n", reg);

    asm volatile ("mov %0, r12" : "=r" (reg));
    print("r12: %p\n", reg);

    asm volatile ("mov %0, sp" : "=r" (reg));
    print("sp : %p\n", reg);

    asm volatile ("mov %0, lr" : "=r" (reg));
    print("lr : %p\n", reg);

    asm volatile ("mov %0, pc" : "=r" (reg));
    print("pc : %p\n", reg);
}

// Declaration of the FPU context switch assembly code
void fpu_context_switch(void);

// Main exeptioon for the undefined exception - PC is wrong...
void undef_exception(u32 pc)
{    
    // Check wheather the undef exception was caused by a FPU access
    u32 fpexc;
    asm volatile ("vmrs %0, fpexc" : "=r"(fpexc) : : "memory");

    if (fpexc & (1 << 29)) {
        // DEX bit is set
        panic("FPU DEX bit set");
    }
    if (fpexc & (1 << 30)) {
        // FPU is already enabled - this is most likely a real UNDEF exeption
        panic("FPU exception with FPU enabled");
    }

    // Try to enable the FPU - since the FPU is off
    fpexc |= (1 << 30);
    asm ("vmsr fpexc, %0" : : "r"(fpexc));
    asm ("dsb");

    // At this point the FPU is enabled. We have to unstack the VFP register for
    // the current running thread since it is going to use the FPU. Because we
    // are implementing a fully lazy FPU context we need to check if the 
    // previous FPU user has any unstacked FPU register in the bank. If so we
    // need to stack these before unstacking the next thread
    fpu_context_switch();
}

void prefetch_exception(u32 pc)
{
    print_regs();

    print("\nKernel fault: prefetch exception\n");
    print("PC\t[%p]\n", pc);

    // The SPSR_abort holds the CPSR of the source thread
    u32 cpsr;
    asm volatile ("mrs %0, spsr" : "=r" (cpsr));
    print("CPSR\t[%p]\n", cpsr);
    
    // Get information about the prefetch abort
    u32 dfsr;
    asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r" (dfsr));
    print("IFSR\t[%p]\n", dfsr);

    u32 status = (dfsr & 0xF) | (((dfsr >> 12) & 1) << 5) | 
        (((dfsr >> 10) & 1) << 4);

    if (status & (1 << 11)) {
        print("Error: W\n");
    } else {
        print("Error: R\n");
    }
    // Decode the status message
    if (status == 0b000001) {
        print("Alginment fault\n");
    } else if (status == 0b000100) {
        print("Instruction cache maintainance fault\n");
    } else if ((status & 0x1F) == 0b01100) {
        print("Level 1 translation sync external abort\n");
    } else if ((status & 0x1F) == 0b01110) {
        print("Level 2 translation sync external abort\n");
    } else if (status == 0b000101) {
        print("Translation fault section\n");
    } else if (status == 0b000111) {
        print("Translation fault page\n");
    } else if (status == 0b000011) {
        print("Access fault section\n");
    } else if (status == 0b000110) {
        print("Access fault page\n");
    } else if (status == 0b001001) {
        print("Domain fault section\n");
    } else if (status == 0b001011) {
        print("Domain fault page\n");
    } else if (status == 0b001101) {
        print("Permission fault section\n");
    } else if (status == 0b001111) {
        print("Permission fault page\n");
    } else if ((status & 0x1F) == 0b01000) {
        print("Sync external abort nontranslation\n");
    } else if ((status & 0x1F) == 0b10110) {
        print("Sync external abort\n");
    } else if (status == 0b001011) {
        print("Domain fault page\n");
    }
    if (status & (1 << 5)) {
        print("AXI slave error caused the abort\n");
    } else {
        print("AXI decode error caused the abort\n");
    }

    u32 ifar;
    asm volatile ("mrc p15, 0, %0, c6, c0, 2" : "=r" (ifar));
    print("Instruction fault addr: %p\n", ifar);

    u32 par;
    asm volatile ("mrc p15, 0, %0, c7, c4, 0" : "=r" (par));
    print("Phys addr: %p\n", par);

    reboot();
    while (1);
}

extern struct rq rq;

void data_exception(u32 pc)
{
    print("Next => %p\n", rq.next);
    print("Curr => %p\n", rq.curr);

    print_regs();

    print("\nKernel fault: data exception\n");
    print("PC\t[%p]\n", pc);

    // The SPSR_abort holds the CPSR of the source thread
    u32 cpsr;
    asm volatile ("mrs %0, spsr" : "=r" (cpsr));
    print("CPSR\t[%p]\n", cpsr);
    
    // Get information about the data abort
    u32 dfsr;
    asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (dfsr));
    print("DFSR\t[%p]\n", dfsr);

    u32 status = (dfsr & 0xF) | (((dfsr >> 12) & 1) << 5) | 
        (((dfsr >> 10) & 1) << 4);

    if (status & (1 << 11)) {
        print("Error: W\n");
    } else {
        print("Error: R\n");
    }
    // Decode the status message
    if (status == 0b000001) {
        print("Alginment fault\n");
    } else if (status == 0b000100) {
        print("Instruction cache maintainance fault\n");
    } else if ((status & 0x1F) == 0b01100) {
        print("Level 1 translation sync external abort\n");
    } else if ((status & 0x1F) == 0b01110) {
        print("Level 2 translation sync external abort\n");
    } else if (status == 0b000101) {
        print("Translation fault section\n");
    } else if (status == 0b000111) {
        print("Translation fault page\n");
    } else if (status == 0b000011) {
        print("Access fault section\n");
    } else if (status == 0b000110) {
        print("Access fault page\n");
    } else if (status == 0b001001) {
        print("Domain fault section\n");
    } else if (status == 0b001011) {
        print("Domain fault page\n");
    } else if (status == 0b001101) {
        print("Permission fault section\n");
    } else if (status == 0b001111) {
        print("Permission fault page\n");
    } else if ((status & 0x1F) == 0b01000) {
        print("Sync external abort nontranslation\n");
    } else if ((status & 0x1F) == 0b10110) {
        print("Sync external abort\n");
    } else if (status == 0b001011) {
        print("Domain fault page\n");
    }
    if (status & (1 << 5)) {
        print("AXI slave error caused the abort\n");
    } else {
        print("AXI decode error caused the abort\n");
    }

    u32 ifar;
    asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (ifar));
    print("Instruction fault addr: %p\n", ifar);

    u32 par;
    asm volatile ("mrc p15, 0, %0, c7, c4, 0" : "=r" (par));
    print("Phys addr: %p\n", par);

    reboot();
    while (1);
}
