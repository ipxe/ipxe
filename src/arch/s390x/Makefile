# Specify compressor
#
ZBIN		= $(ZBIN64)

# Assembler section type character
#
ASM_TCHAR	:= @
ASM_TCHAR_OPS	:= @

# S/390-specific flags
#
CFLAGS		+= -mno-backchain -mpacked-stack -msoft-float

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
#
CFLAGS		+= -fshort-wchar

# Include S/390-specific headers
#
INCDIRS		:= arch/$(ARCH)/include $(INCDIRS)

# S/390-specific directories containing source files
#
SRCDIRS		+= arch/s390x/core

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/s390x/Makefile.$(PLATFORM)
include arch/s390x/Makefile.$(PLATFORM)
