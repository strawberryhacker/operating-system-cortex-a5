# Copyright (C) strawberryhacker

TOP         = $(shell pwd)
BUILDDIR    = $(TOP)/build
TARGET_NAME = malloc
MAKEFLAGS   = -j6
COM_PORT    = /dev/ttyS4

# Compilers
CC      = arm-none-eabi-gcc
OBJDUMP = arm-none-eabi-objdump
OBJCOPY = arm-none-eabi-objcopy
ARM_ASM = arm-none-eabi-as
GDB     = arm-none-eabi-gdb

# Set the compiler flags
CFLAGS   += -x c -O2 -ffunction-sections -fdata-sections -mlong-calls -Wall
CFLAGS   += -std=gnu99 -mcpu=cortex-a5 -c -s

# Supress warnings
CFLAGS   += -Wno-unused-function -Wno-unused-variable

LDFLAGS  += -shared
LDFLAGS  += -Wl,--gc-sections -mcpu=cortex-a5
LDFLAGS  += -Wl,-Map="$(BUILDDIR)/$(TARGET_NAME).map" -s

# flto
ASMFLAGS += -mcpu=cortex-a5

# Include all the project makefiles. This will add all object files and all 
# include paths to the OBJ and CPPFLAGS respectively
include-flags-y :=
linker-script-y :=
obj-y :=

obj-y += /malloc.o

# All object files are addes so we place them in the build variable
BUILDOBJ = $(addprefix $(BUILDDIR), $(obj-y))
CPFLAGS += $(include-flags-y)
CPFLAGS += -I.


.SECONDARY: $(BUILDOBJ)
.PHONY: all elf clean 
all: lss

# Subtargets
elf: $(BUILDDIR)/$(TARGET_NAME).elf
lss: $(BUILDDIR)/$(TARGET_NAME).lss
bin: $(BUILDDIR)/$(TARGET_NAME).bin
lib: $(BUILDDIR)/$(TARGET_NAME).so

$(BUILDDIR)/%.so: $(BUILDOBJ)
	@echo
	@$(CC) $(LDFLAGS) $^ -o $@
	@echo

$(BUILDDIR)/%.elf: $(BUILDOBJ)
	@echo
	@$(CC) $(LDFLAGS) -Wl,--print-memory-usage $^ -o $@
	@echo

$(BUILDDIR)/%.lss: $(BUILDDIR)/%.elf
	@$(OBJDUMP) -h -S $< > $@

$(BUILDDIR)/%.bin: $(BUILDDIR)/%.elf
	@$(OBJCOPY) -O binary $< $@
	@echo Build completed
	@echo

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo " >" $<
	@$(CC) $(CFLAGS) $(CPFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo " >" $<
	@$(ARM_ASM) $(ASMFLAGS) -c $< -o $@	

clean:
	@rm -r -f $(BUILDDIR)
