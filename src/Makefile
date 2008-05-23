# Location to place generated files
#
BIN		:= bin

# Initialise variables that get added to throughout the various Makefiles
#
MAKEDEPS	:= Makefile .toolcheck .echocheck
SRCDIRS		:=
SRCS		:=
NON_AUTO_SRCS	:=
DRIVERS		:=
ROMS		:=
MEDIA		:=
NON_AUTO_MEDIA	:=

# Locations of utilities
#
HOST_CC		:= gcc
RM		:= rm -f
TOUCH		:= touch
MKDIR		:= mkdir
CP		:= cp
ECHO		:= echo
PRINTF		:= printf
PERL		:= /usr/bin/perl
CC		:= $(CROSS_COMPILE)gcc
CPP		:= $(CROSS_COMPILE)gcc -E -Wp,-Wall
AS		:= $(CROSS_COMPILE)as
LD		:= $(CROSS_COMPILE)ld
SIZE		:= $(CROSS_COMPILE)size
AR		:= $(CROSS_COMPILE)ar
RANLIB		:= $(CROSS_COMPILE)ranlib
OBJCOPY		:= $(CROSS_COMPILE)objcopy
NM		:= $(CROSS_COMPILE)nm
OBJDUMP		:= $(CROSS_COMPILE)objdump
PARSEROM	:= $(PERL) ./util/parserom.pl
MAKEROM		:= $(PERL) ./util/makerom.pl
MKCONFIG	:= $(PERL) ./util/mkconfig.pl
SYMCHECK	:= $(PERL) ./util/symcheck.pl
SORTOBJDUMP	:= $(PERL) ./util/sortobjdump.pl
NRV2B		:= ./util/nrv2b
ZBIN		:= ./util/zbin
DOXYGEN		:= doxygen

# If invoked with no build target, print out a helpfully suggestive
# message.
#
noargs : blib $(BIN)/NIC $(BIN)/gpxe.dsk $(BIN)/gpxe.iso $(BIN)/gpxe.usb
	@$(ECHO) '==========================================================='
	@$(ECHO)
	@$(ECHO) 'To create a bootable floppy, type'
	@$(ECHO) '    cat $(BIN)/gpxe.dsk > /dev/fd0'
	@$(ECHO) 'where /dev/fd0 is your floppy drive.  This will erase any'
	@$(ECHO) 'data already on the disk.'
	@$(ECHO)
	@$(ECHO) 'To create a bootable USB key, type'
	@$(ECHO) '    cat $(BIN)/gpxe.usb > /dev/sdX'
	@$(ECHO) 'where /dev/sdX is your USB key, and is *not* a real hard'
	@$(ECHO) 'disk on your system.  This will erase any data already on'
	@$(ECHO) 'the USB key.'
	@$(ECHO)
	@$(ECHO) 'To create a bootable CD-ROM, burn the ISO image '
	@$(ECHO) '$(BIN)/gpxe.iso to a blank CD-ROM.'
	@$(ECHO)
	@$(ECHO) 'These images contain drivers for all supported cards.  You'
	@$(ECHO) 'can build more customised images, and ROM images, using'
	@$(ECHO) '    make bin/<rom-name>.<output-format>'
	@$(ECHO)
	@$(ECHO) '==========================================================='

# Grab the central Config file.
#
MAKEDEPS	+= Config
include Config

# If no architecture is specified in Config or on the command-line,
# use that of the build machine.
#
ARCH		?= $(shell uname -m | sed -e s,i[3456789]86,i386,)

# handle x86_64 like i386, but set -m32 option for 32bit code only
ifeq ($(ARCH),x86_64)
ARCH		:= i386
CFLAGS		+= -m32
ASFLAGS         += --32
LDFLAGS         += -m elf_i386
endif

# Drag in architecture-specific Config
#
MAKEDEPS	+= arch/$(ARCH)/Config
include arch/$(ARCH)/Config

# Common flags
#
CFLAGS		+= -I include -I arch/$(ARCH)/include -I . -DARCH=$(ARCH)
CFLAGS		+= -Os -ffreestanding
CFLAGS		+= -Wall -W
CFLAGS		+= -g
CFLAGS		+= $(EXTRA_CFLAGS)
ASFLAGS		+= $(EXTRA_ASFLAGS)
LDFLAGS		+= $(EXTRA_LDFLAGS)

# Embedded image, if present
#
EMBEDDED_IMAGE	?= /dev/null

ifneq ($(NO_WERROR),1)
CFLAGS		+= -Werror
endif

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
	@$(ECHO) $(OBJ_CFLAGS)

# Rules for specific object types.
#
COMPILE_c	= $(CC) $(CFLAGS) $(CFLAGS_c) $(OBJ_CFLAGS)
RULE_c		= $(Q)$(COMPILE_c) -c $< -o $@
RULE_c_to_dbg%.o = $(Q)$(COMPILE_c) -Ddebug_$(OBJECT)=$* -c $< -o $@
RULE_c_to_c	= $(Q)$(COMPILE_c) -E -c $< > $@
RULE_c_to_s	= $(Q)$(COMPILE_c) -S -g0 -c $< -o $@ 

PREPROCESS_S	= $(CPP) $(CFLAGS) $(CFLAGS_S) $(OBJ_CFLAGS)
ASSEMBLE_S	= $(AS) $(ASFLAGS)
RULE_S		= $(Q)$(PREPROCESS_S) $< | $(ASSEMBLE_S) -o $@
RULE_S_to_s	= $(Q)$(PREPROCESS_S) $< > $@

DEBUG_TARGETS	+= dbg%.o c s

# SRCDIRS lists all directories containing source files.
#
SRCDIRS		+= libgcc
SRCDIRS		+= core
SRCDIRS		+= proto
SRCDIRS		+= net net/tcp net/udp
SRCDIRS		+= image
SRCDIRS		+= drivers/bus
SRCDIRS		+= drivers/net
SRCDIRS		+= drivers/net/e1000
SRCDIRS		+= drivers/block
SRCDIRS		+= drivers/nvs
SRCDIRS		+= drivers/bitbash
SRCDIRS		+= drivers/infiniband
SRCDIRS		+= interface/pxe
SRCDIRS		+= tests
SRCDIRS		+= crypto crypto/axtls crypto/matrixssl
SRCDIRS		+= hci hci/commands hci/tui
SRCDIRS		+= hci/mucurses hci/mucurses/widgets
SRCDIRS		+= usr

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
		  -i$(IDENT) -s 0 $@

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
