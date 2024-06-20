# Assembler section type character
#
ASM_TCHAR	:= @
ASM_TCHAR_OPS	:= @

# LoongArch64-specific flags
#
CFLAGS		+= -fstrength-reduce -fomit-frame-pointer
CFLAGS		+= -falign-jumps=1 -falign-loops=1 -falign-functions=1

## Check if the follow options is valid for current toolchains
# -mno-lsx: Disable 128-bit SIMD instructions, added after gcc-14
# -mno-lasx: Disable 256-bit SIMD instructions, added after gcc-14
# -mno-relax: Disable linker relaxation, added after gcc-14
# -Wa,-mno-relax: Disable linker relaxation, added after binutils-2.41
CHECK_CC_FLAGS := -mno-lsx -mno-lasx -mno-relax -Wa,-mno-relax

define check_cc_flag
  $(shell $(CC) $(1) -x c -c /dev/null -o /dev/null > /dev/null 2>&1 && echo $(1))
endef
CFLAGS += $(foreach flag,$(CHECK_CC_FLAGS),$(call check_cc_flag,$(flag)))

# Check if -mno-explicit-relocs is valid
ifeq ($(CCTYPE),gcc)
MNER_TEST = $(CC) -mno-explicit-relocs -x c -c /dev/null -o /dev/null >/dev/null 2>&1
MNER_FLAGS := $(shell $(MNER_TEST) && $(ECHO) '-mno-explicit-relocs')
WORKAROUND_CFLAGS += $(MNER_FLAGS)
endif

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
CFLAGS		+= -fshort-wchar

# LoongArch64-specific directories containing source files
SRCDIRS		+= arch/loong64/core
SRCDIRS		+= arch/loong64/interface/efi

# Include platform-specific Makefile
MAKEDEPS	+= arch/loong64/Makefile.$(PLATFORM)
include arch/loong64/Makefile.$(PLATFORM)
