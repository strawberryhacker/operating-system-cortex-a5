# Copyright (C) strawberryhacker

TOP         = $(shell pwd)
BUILDDIR    = $(TOP)/build
TARGET_NAME = citrus
MAKEFLAGS   = -j6
COM_PORT    = /dev/ttyS4

# ---------------------------------------------------------------------------
# Compile flags (this should problably be defined based on the architecture)

# Compilers
CC      = arm-none-eabi-gcc
OBJDUMP = arm-none-eabi-objdump
OBJCOPY = arm-none-eabi-objcopy
ARM_ASM = arm-none-eabi-as
GDB     = arm-none-eabi-gdb

# Set the compiler flags
CFLAGS   += -x c -O1 -g3 -fdata-sections -mlong-calls -Wall
CFLAGS   += -std=gnu99 -mcpu=cortex-a5 -c -ffunction-sections

# Supress warnings
CFLAGS   += -Wno-unused-function -Wno-unused-variable
CFLAGS   += -ffreestanding

LDFLAGS  += -Wl,--start-group -Wl,--end-group
LDFLAGS  += -Wl,--gc-sections -mcpu=cortex-a5
LDFLAGS  += -Wl,-Map="$(BUILDDIR)/$(TARGET_NAME).map"

ASMFLAGS += -mcpu=cortex-a5 -g3

# ---------------------------------------------------------------------------
# Include symbols and variables 

# Include all the project makefiles. This will add all object files and all 
# include paths to the OBJ and CPPFLAGS respectively
include-flags-y :=
linker-script-y :=
obj-y :=

include $(TOP)/entry/Makefile
include $(TOP)/include/Makefile
include $(TOP)/arch/Makefile
include $(TOP)/lib/Makefile
include $(TOP)/drivers/Makefile
include $(TOP)/mm/Makefile
include $(TOP)/benchmark/Makefile
include $(TOP)/kernel/Makefile
include $(TOP)/fs/Makefile
include $(TOP)/app/Makefile
include $(TOP)/graphics/Makefile

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

# ---------------------------------------------------------------------------
# Rules

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

# ---------------------------------------------------------------------------
# Connect to custom program loader for c-boot

install: all
	@python3 $(TOP)/tools/kernel_load.py -c $(COM_PORT) -f $(BUILDDIR)/$(TARGET_NAME).bin

app:
	@cd $(TOP)/application && $(MAKE) -s clean && $(MAKE) -s
	@python3 $(TOP)/tools/app_load.py -c $(COM_PORT) -f $(TOP)/application/build/citrus.elf

debug: install
	$(GDB) -f $(BUILDDIR)/$(TARGET_NAME).elf -x debug/debug.gdb

clean:
	@rm -r -f $(BUILDDIR)
