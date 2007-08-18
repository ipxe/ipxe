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

# Location to place generated files
#
BIN		:= bin

# If no architecture is specified in Config or on the command-line,
# use that of the build machine.
#
ifndef ARCH
ARCH		:= $(shell uname -m | sed -e s,i[3456789]86,i386,)
endif

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

# If invoked with no build target, print out a helpfully suggestive
# message.
#
noargs : blib $(BIN)/NIC $(BIN)/gpxe.dsk $(BIN)/gpxe.iso $(BIN)/gpxe.usb
	@echo '==========================================================='
	@echo
	@echo 'To create a bootable floppy, type'
	@echo '    cat $(BIN)/gpxe.dsk > /dev/fd0'
	@echo 'where /dev/fd0 is your floppy drive.  This will erase any'
	@echo 'data already on the disk.'
	@echo
	@echo 'To create a bootable USB key, type'
	@echo '    cat $(BIN)/gpxe.usb > /dev/sdX'
	@echo 'where /dev/sdX is your USB key, and is *not* a real hard'
	@echo 'disk on your system.  This will erase any data already on'
	@echo 'the USB key.'
	@echo
	@echo 'To create a bootable CD-ROM, burn the ISO image '
	@echo '$(BIN)/gpxe.iso to a blank CD-ROM.'
	@echo
	@echo 'These images contain drivers for all supported cards.  You'
	@echo 'can build more customised images, and ROM images, using'
	@echo '    make bin/<rom-name>.<output-format>'
	@echo
	@echo '==========================================================='

# Locations of utilities
#
HOST_CC		?= gcc
CPP		?= gcc -E -Wp,-Wall
RM		?= rm -f
TOUCH		?= touch
MKDIR		?= mkdir
CP		?= cp
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
ZBIN		?= ./util/zbin
DOXYGEN		?= doxygen

# Common flags
#
CFLAGS		+= -I include -I arch/$(ARCH)/include -I . -DARCH=$(ARCH)
CFLAGS		+= -Os -ffreestanding
CFLAGS		+= -Wall -W
CFLAGS		+= -g
CFLAGS		+= $(EXTRA_CFLAGS)
ASFLAGS		+= $(EXTRA_ASFLAGS)
LDFLAGS		+= $(EXTRA_LDFLAGS)

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
	@echo $(OBJ_CFLAGS)

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
SRCDIRS		+= drivers/block
SRCDIRS		+= drivers/scsi
SRCDIRS		+= drivers/ata
SRCDIRS		+= drivers/nvs
SRCDIRS		+= drivers/bitbash
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

SRCS		+= drivers/net/mlx_ipoib/mt25218.c
SRCS		+= drivers/net/mlx_ipoib/mt23108.c
CFLAGS_mt25218	= -Wno-error
CFLAGS_mt23108	= -Wno-error

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
