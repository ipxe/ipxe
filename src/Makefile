# Initialise variables that get added to throughout the various Makefiles
#
MAKEDEPS	:= Makefile .toolcheck
SRCDIRS		:=
SRCS		:=
NON_AUTO_SRCS	:=
DRIVERS		:=
ROMS		:=
MEDIA		:=
NON_AUTO_MEDIA	:=

# Grab the central Config file.
#
MAKEDEPS	+= Config
include Config

# If no architecture is specified in Config or on the command-line,
# use that of the build machine.
#
ifndef ARCH
ARCH		:= $(shell uname -m | sed -e s,i[3456789]86,i386,)
endif

# Drag in architecture-specific Config
#
MAKEDEPS	+= arch/$(ARCH)/Config
include arch/$(ARCH)/Config

# If invoked with no build target, print out a helpfully suggestive
# message.
#
noargs : blib
	@echo '===================================================='
	@echo 'No target specified. To specify a target, do: '
	@echo
	@echo '    $(MAKE) bin/<rom-name>.<output-format> '
	@echo
	@echo 'where <output-format> is one of [z]{$(MEDIA) }'
	@echo
	@echo 'or: '
	@echo
	@echo '    $(MAKE) all<output-format>s'
	@echo
	@echo 'to generate all possible images of format <output-format>'
	@echo
	@echo 'For example, '
	@echo
	@echo '    make allzroms '
	@echo
	@echo 'will generate all possible .zrom (rom burnable) images, and'
	@echo
	@echo '    make allzdsks'
	@echo
	@echo 'will generate all possible .zdsk (bootable floppy) images, or'
	@echo
	@echo '===================================================='
	@exit 1

# Locations of utilities
#
HOST_CC		?= gcc
CPP		?= gcc -E -Wp,-Wall
RM		?= rm -f
TOUCH		?= touch
MKDIR		?= mkdir
PERL		?= /usr/bin/perl
CC		?= $(CROSS_COMPILE)gcc
AS		?= $(CROSS_COMPILE)as
LD		?= $(CROSS_COMPILE)ld
SIZE		?= $(CROSS_COMPILE)size
AR		?= $(CROSS_COMPILE)ar
RANLIB		?= $(CROSS_COMPILE)ranlib
OBJCOPY		?= $(CROSS_COMPILE)objcopy
NM		?= $(CROSS_COMPILE)nm
OBJDUMP		?= $(CROSS_COMPILE)objdump
PARSEROM	?= $(PERL) ./util/parserom.pl
MAKEROM		?= $(PERL) ./util/makerom.pl
MKCONFIG	?= $(PERL) ./util/mkconfig.pl
SYMCHECK	?= $(PERL) ./util/symcheck.pl
SORTOBJDUMP	?= $(PERL) ./util/sortobjdump.pl
NRV2B		?= ./util/nrv2b

# Location to place generated files
#
BIN		?= bin

# Common flags
#
CFLAGS		+= -I include -I arch/$(ARCH)/include -I . -DARCH=$(ARCH)
CFLAGS		+= -Os -ffreestanding
CFLAGS		+= -Wall -W -Wno-format
CFLAGS		+= $(EXTRA_CFLAGS)
ASFLAGS		+= $(EXTRA_ASFLAGS)
LDFLAGS		+= $(EXTRA_LDFLAGS)

# CFLAGS for specific object types
#
CFLAGS_c	+= 
CFLAGS_S 	+= -DASSEMBLY

# Base object name of the current target
#
OBJECT		= $(firstword $(subst ., ,$(@F)))

# CFLAGS for specific object files.  You can define
# e.g. CFLAGS_rtl8139, and have those flags automatically used when
# compiling bin/rtl8139.o.
#
OBJ_CFLAGS	= $(CFLAGS_$(OBJECT)) -DOBJECT=$(subst -,_,$(OBJECT))
$(BIN)/%.flags :
	@echo $(OBJ_CFLAGS)

# Rules for specific object types.
#
COMPILE_c	= $(CC) $(CFLAGS) $(CFLAGS_c) $(OBJ_CFLAGS)
RULE_c		= $(COMPILE_c) -c $< -o $@
RULE_c_to_dbg.o	= $(COMPILE_c) -Ddebug_$(OBJECT) -c $< -o $@
RULE_c_to_dbg2.o = $(COMPILE_c) -Ddebug_$(OBJECT)=2 -c $< -o $@
RULE_c_to_c	= $(COMPILE_c) -E -c $< > $@
RULE_c_to_s	= $(COMPILE_c) -S -c $< -o $@ 

PREPROCESS_S	= $(CPP) $(CFLAGS) $(CFLAGS_S) $(OBJ_CFLAGS)
ASSEMBLE_S	= $(AS) $(ASFLAGS)
RULE_S		= $(PREPROCESS_S) $< | $(ASSEMBLE_S) -o $@
RULE_S_to_s	= $(PREPROCESS_S) $< > $@

DEBUG_TARGETS	+= dbg2.o dbg.o c s

# SRCDIRS lists all directories containing source files.
#
SRCDIRS		+= core
SRCDIRS		+= drivers/bus
SRCDIRS		+= drivers/net
#SRCDIRS		+= drivers/disk 

# NON_AUTO_SRCS lists files that are excluded from the normal
# automatic build system.
#
NON_AUTO_SRCS	+= core/elf_loader.c
NON_AUTO_SRCS	+= drivers/net/prism2.c

# Rules for finalising files.  TGT_MAKEROM_FLAGS is defined as part of
# the automatic build system and varies by target; it includes the
# "-p 0x1234,0x5678" string to set the PCI IDs.
#
FINALISE_rom	= $(MAKEROM) $(MAKEROM_FLAGS) $(TGT_MAKEROM_FLAGS) \
		  -i$(IDENT) $@

# Some ROMs require specific flags to be passed to makerom.pl
#
MAKEROM_FLAGS_3c503 = -3

# Drag in architecture-specific Makefile
#
MAKEDEPS	+= arch/$(ARCH)/Makefile
include arch/$(ARCH)/Makefile

# Drag in the automatic build system and other housekeeping functions
MAKEDEPS	+= Makefile.housekeeping
include Makefile.housekeeping
