# Copyright (C) strawberryhacker

TOP         = $(shell pwd)
BUILDDIR    = $(TOP)/build
TARGET_NAME = app
#MAKEFLAGS   = -j6
COM_PORT    = /dev/ttyS4

# Compilers
CC      = arm-none-eabi-gcc
OBJDUMP = arm-none-eabi-objdump
OBJCOPY = arm-none-eabi-objcopy
ARM_ASM = arm-none-eabi-as
GDB     = arm-none-eabi-gdb

# Set the compiler flags
CFLAGS   += -x c -O2 -ffunction-sections -fdata-sections -mlong-calls -Wall
CFLAGS   += -std=gnu99 -mcpu=cortex-a5 -c

# Supress warnings
CFLAGS   += -Wno-unused-function -Wno-unused-variable

LDFLAGS  += -Wl,--start-group -Wl,--end-group
LDFLAGS  += -Wl,--gc-sections -mcpu=cortex-a5
LDFLAGS  += -Wl,-Map="$(BUILDDIR)/$(TARGET_NAME).map"

# flto
ASMFLAGS += -mcpu=cortex-a5

# Include all the project makefiles. This will add all object files and all 
# include paths to the OBJ and CPPFLAGS respectively
include-flags-y :=
linker-script-y :=
obj-y :=

include $(TOP)/entry/Makefile
include $(TOP)/include/Makefile
include $(TOP)/lib/Makefile


# Check that the linker script is provided
ifneq ($(MAKECMDGOALS),clean)
ifndef linker-script-y
$(error linker script is not provided)
endif
endif

# All object files are addes so we place them in the build variable
BUILDOBJ = $(addprefix $(BUILDDIR), $(obj-y))
CPFLAGS += $(include-flags-y)
CPFLAGS += -I.
LDFLAGS += -T$(linker-script-y)

.SECONDARY: $(BUILDOBJ)
.PHONY: all elf install reinstall debug clean 
all: elf lss bin

# Subtargets
elf: $(BUILDDIR)/$(TARGET_NAME).elf
lss: $(BUILDDIR)/$(TARGET_NAME).lss
bin: $(BUILDDIR)/$(TARGET_NAME).bin

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
