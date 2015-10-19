# Assembler section type character
#
ASM_TCHAR	:= %
ASM_TCHAR_OPS	:= %%

# ARM-specific directories containing source files
#
SRCDIRS		+= arch/arm/core
SRCDIRS		+= arch/arm/libgcc
SRCDIRS		+= arch/arm/interface/efi

# ARM-specific flags
#
CFLAGS		+= -mthumb -mcpu=cortex-a15 -mabi=aapcs -mfloat-abi=soft
CFLAGS		+= -mword-relocations
ASFLAGS		+= -mthumb -mcpu=cortex-a15

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
#
CFLAGS		+= -fshort-wchar

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/arm/Makefile.$(PLATFORM)
include arch/arm/Makefile.$(PLATFORM)
