# Specify compressor
#
ZBIN		= $(ZBIN32)

# RISCV32-specific directories containing source files
#
SRCDIRS		+= arch/riscv32/core
SRCDIRS		+= arch/riscv32/libgcc

# RISCV32-specific flags
#
CFLAGS		+= -march=rv32gc -mabi=ilp32d
ASFLAGS		+= -march=rv32gc -mabi=ilp32d
LDFLAGS		+= -m elf32lriscv

# Include common RISCV Makefile
#
MAKEDEPS	+= arch/riscv/Makefile
include arch/riscv/Makefile

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/riscv32/Makefile.$(PLATFORM)
include arch/riscv32/Makefile.$(PLATFORM)
