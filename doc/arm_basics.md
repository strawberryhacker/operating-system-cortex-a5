## ARM basics

This document will explain some key elements in the ARM architecture. This will focus on ARMv7 microprocessors and NOT microcontrollers.

## Modes and privileges

The classical ARM architecture has seven different modes (this exludes hypervisor and security extensions)

 - User
 - FIQ
 - IRQ
 - Supervisor
 - Abort
 - Undef
 - System

Only the user mode is non-privileged while the other run in privileged mode. If hypervisor extensions are implemented the privilege levels change to PL levels (just a heads up). Changeing the processor mode can happend two ways

 - by taking an exception
 - by explicitly modifying the CPSR in privileged mode
 

As should be known the ARMv7 Cortex-A processor contains 16 general purpuse register plus the CPSR. R13 is the stack pointer, R14 is the link register and R15 is the program counter. The CPSR contains important information of the system. There are two instances of this register; the CPSR which hold the status for the current execution mode, and the SPSR which holds the status from the previous execution mode. In user mode the CPSR are not available and the ACSR is accessed instead which contains a subset of the CPSR. The CPSR contains the following

 - The APSR flags N, Z, C, V, Q for code execution and GE bits used in SIMD computing
 - Current CPU mode
 - Endian
 - IT state used in Thumb-2
 - CPU state usally ARM or Thumb
 - Interrupt disable flags
 
Other registers do also have different instanses of the same register. This depends on the execution mode of the processor and is called register banking. The low registers and PC are NOT banked. The SP and CPSR are banked in all modes except system and user. Therefore any functional boot code must initialize the stack pointer for all the modes. 


## Exceptions

The Cortex-A series has 7 exception vectors and thus 7 exceptions (assumed virtualization and security extension are disabled). Two of these, called IRQ and FIQ are interrupts. They are vey much alike in general, but uses different terminology. 

The **interrupt** part of a Cortex consist of two interrupt lines into the core. If any of these are asserted the core will take an exception after the previous instructions finishes. Typically a interrupt controller such as the APIC will process all the interrupt and provide a single serialize signal on each of the interrupt lines; IRQ and FIQ. The difference between they are the priority and the speed. The FIQ has higher priority and contains a large number of banked registers, thus speeding up the exception.

**Aborts** can either be prefetch abort (in case of instruction fetch failure) or data abort (in case of data fetch failure). The prefetch abort exception are only taken if the processor tries to run it. This is because of the multi-stage pipeline of the processor. Before the kernel switches thread the A flag must be cleared, protected by a barrier instruction, so that any asynchronous aborts will be detected. If the abort is imprecise the kernel MUST kill the thread running. 

**Reset** is also an exception in the core. This is not maskable, has highest priority and runs in supervisor mode (or SVC).

Both the FIQ and the reset excpetion disables FIQ and IRQ by setting the I and F flag in the CPSR. The rest of the exceptions only disable IRQ. **NOTE** that some of the interrupt handlers needs to modify the LR value upon entering the exception. SRS can be used to push LR and CPSR onto stack. When taking an exception the core does the following

### Exception entry

 - Copies CPSR to the SPSR_xxx where xxx denotes the new exception mode
 - Updated LR
 - Updates MODE bits in CPSR
 - Updates T and J bits in CPSR (only if exceptions run in Thumb mode or CP15 TE bit is set)
 - Updates PC to an entry in the vector table
 
### Exception return
 
 - Restore CPSR from SPSR_xxx
 - Set PC to the return address
 
This MUST happend atomically. 
