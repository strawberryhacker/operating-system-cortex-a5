// Copyright (C) strawberryhacker 

#include <citrus/apic.h>
#include <citrus/matrix.h>
#include <citrus/regmap.h>

#define PERIPH_CNT 72

// Remaps all secure interrupts to the non-secure APIC controller
static void apic_secure_remap(void)
{
    u32 key = SFR->SN1 ^ 0xB6D81C4D;
    SFR->AICREDIR = key | 1;
}

// Returns the correct APIC controller (secure / non-secure) based on the IRQ 
// number. This will also check the state of the SFR redirection bits
static struct apic_reg* get_apic(u8 irq)
{
    struct matrix_reg* matrix = get_bus(irq);
    if (is_secure(matrix, irq)) {
        // If secure interrupts are redirected - force the APIC controller 
        if (SFR->AICREDIR == 0) {
            return SAPIC;
        }
    }
    return APIC;
}

// Dummy handler for unconfigured interrupts
static void dummy_handler(void)
{
    asm volatile ("bkpt #0");
}

// Initialize the APIC controller and redirect all secure interrupt to the 
// non-secure interrupt controller. This will reconfigure the APIC from the
// bootloader causing the dummy handler in the kernel space to be called
static void _apic_init(struct apic_reg* apic)
{
    for (u8 i = 1; i < PERIPH_CNT; i++) {
        apic->SSR = i;
        apic->IDCR = 1;
        apic->ICCR = 1;
    }

    // Acknowledge all the stacked interrupts 
    for (u8 i = 0; i < 8; i++) {
        apic->EOICR = 0;
    }

    for (u8 i = 0; i < PERIPH_CNT; i++) {
        apic->SSR = i;
        apic->SVR = (u32)dummy_handler;
    }
    apic->SPU = (u32)dummy_handler;
}

// Enables protect mode. This mode should be used when a debugger is attatched.
// Warning: this has concequences in the interrupt handler. Look at a 
// implementation from Atmel. The solution is NOT specified in the datasheet...
void apic_protect(struct apic_reg* apic)
{
    apic->DCR = 1;
}

// Initialization for the APIC controller. This controller is configured in the
// bootloader but it is reconfigured here in order to remap the dummy_handlers
// to the kernel space
void apic_init(void)
{
    _apic_init(APIC);
    apic_protect(APIC);
    _apic_init(SAPIC);
    apic_secure_remap();
}

// Adds a interrupt handler to the APIC. This will be called when the interrupt
// is scheduled for execution
void apic_add_handler(u32 irq, void (*handler)(void))
{
    struct apic_reg* apic = get_apic(irq);

    apic->SSR = irq;
    apic->IDCR = 1;
    apic->SMR = (1 << 5);
    apic->SVR = (u32)handler;
    apic->ICCR = 1;
}

void apic_remove_handler(u32 irq)
{
    struct apic_reg* apic = get_apic(irq);

    apic->SSR = irq;
    apic->IDCR = 1;
    apic->SMR = (1 << 5);
    apic->SVR = (u32)dummy_handler;
    apic->ICCR = 1;
}

// Enables the APIC to assert the IRQ or FIQ line when the peripheral generates
// an interrupt
void apic_enable(u32 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->ICCR = 1;
    apic->IECR = 1;
}

void apic_disable(u32 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->IDCR = 1;
}

void apic_force(u32 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->ISCR = 1;
    asm ("dsb" : : : "memory");
    asm ("dmb" : : : "memory");
    asm ("isb" : : : "memory");
}

void apic_clear(u32 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->ICCR = 1;
}

// Sets the APIC priority. The functions irq_get_max_priority and 
// irq_get_min_priority can be used to get the minimum and maximum number to 
// put in
void apic_set_priority(u32 irq, u32 pri)
{
    struct apic_reg* apic = get_apic(irq);

    apic->SSR = irq;
    apic->IDCR = 1;
    u32 reg = apic->SMR & ~0b111;
    apic->SMR = reg | pri;
    apic->ICCR = 1;
}

u32 apic_get_max_priority(u32 irq)
{
    return 7;
}

u32 apic_get_min_priority(u32 irq)
{
    return 0;
}
