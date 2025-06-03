# Specify compressor
#
ZBIN		= $(ZBIN64)

# RISCV64-specific directories containing source files
#
SRCDIRS		+= arch/riscv64/core

# RISCV64-specific flags
#
CFLAGS		+= -march=rv64gc -mabi=lp64d
ASFLAGS		+= -march=rv64gc -mabi=lp64d
LDFLAGS		+= -m elf64lriscv

# Include common RISCV Makefile
#
MAKEDEPS	+= arch/riscv/Makefile
include arch/riscv/Makefile

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/riscv64/Makefile.$(PLATFORM)
include arch/riscv64/Makefile.$(PLATFORM)
