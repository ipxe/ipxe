# ARM64-specific directories containing source files
#
SRCDIRS		+= arch/arm64/core

# ARM64-specific flags
#
CFLAGS		+= -mlittle-endian -mcmodel=small
CFLAGS		+= -fomit-frame-pointer
ASFLAGS		+= -mabi=lp64 -EL

# We want to specify the LP64 model.  There is an explicit -mabi=lp64
# on GCC 4.9 and later, and no guarantee as to which is the default
# model.  In earlier versions of GCC, there is no -mabi option and the
# default appears to be LP64 anyway.
#
ifeq ($(CCTYPE),gcc)
LP64_TEST = $(CC) -mabi=lp64 -x c -c /dev/null -o /dev/null >/dev/null 2>&1
LP64_FLAGS := $(shell $(LP64_TEST) && $(ECHO) '-mabi=lp64')
WORKAROUND_CFLAGS += $(LP64_FLAGS)
endif

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
#
CFLAGS		+= -fshort-wchar

# Include common ARM Makefile
MAKEDEPS	+= arch/arm/Makefile
include arch/arm/Makefile

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/arm64/Makefile.$(PLATFORM)
include arch/arm64/Makefile.$(PLATFORM)
