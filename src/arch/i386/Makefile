# Force i386-only instructions
#
CFLAGS		+= -march=i386

# Code size reduction.
#
CFLAGS		+= -fstrength-reduce -fomit-frame-pointer

# Code size reduction.  gcc3 needs a different syntax to gcc2 if you
# want to avoid spurious warnings.
#
GCC_VERSION	:= $(subst ., ,$(shell $(CC) -dumpversion))
GCC_MAJOR	:= $(firstword $(GCC_VERSION))
ifeq ($(GCC_MAJOR),2)
CFLAGS		+= -malign-jumps=1 -malign-loops=1 -malign-functions=1
else
CFLAGS		+= -falign-jumps=1 -falign-loops=1 -falign-functions=1
endif

# Code size reduction.  This is almost always a win.  The kernel uses it, too.
#
CFLAGS		+= -mpreferred-stack-boundary=2

# Code size reduction.  Use regparm for all functions - C functions
# called from assembly (or vice versa) need __cdecl now
#
CFLAGS		+= -mregparm=3

# Code size reduction.  Use -mrtd (same __cdecl requirements as above)
CFLAGS		+= -mrtd

# Code size reduction.  This is the logical complement to -mregparm=3.
# It doesn't currently buy us anything, but if anything ever tries to
# return small structures, let's be prepared
#
CFLAGS		+= -freg-struct-return

# Stop ld from complaining about our customised linker script
#
LDFLAGS		+= -N --no-check-sections

# Force 32-bit code even on an x86-64 machine
#
CFLAGS		+= -m32
ASFLAGS         += --32
LDFLAGS         += -m elf_i386

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
#
CFLAGS		+= -fshort-wchar

# We need to undefine the default macro "i386" when compiling .S
# files, otherwise ".arch i386" translates to ".arch 1"...
#
CFLAGS			+= -Ui386

# Locations of utilities
#
ISOLINUX_BIN	= /usr/lib/syslinux/isolinux.bin

# i386-specific directories containing source files
#
SRCDIRS		+= arch/i386/core arch/i386/transitions arch/i386/prefix
SRCDIRS		+= arch/i386/firmware/pcbios
SRCDIRS		+= arch/i386/image
SRCDIRS		+= arch/i386/drivers
SRCDIRS		+= arch/i386/drivers/net
SRCDIRS		+= arch/i386/interface/pcbios
SRCDIRS		+= arch/i386/interface/pxe
SRCDIRS 	+= arch/i386/interface/syslinux

# The various xxx_loader.c files are #included into core/loader.c and
# should not be compiled directly.
#
NON_AUTO_SRCS	+= arch/i386/core/aout_loader.c
NON_AUTO_SRCS	+= arch/i386/core/freebsd_loader.c
NON_AUTO_SRCS	+= arch/i386/core/wince_loader.c

# unnrv2b.S is used to generate a 16-bit as well as a 32-bit object.
#
OBJS_unnrv2b		= unnrv2b unnrv2b16
CFLAGS_unnrv2b16	= -DCODE16

# The i386 linker script
#
LDSCRIPT		= arch/i386/scripts/i386.lds

# Media types.
# 
MEDIA			+= rom
MEDIA			+= pxe
MEDIA			+= kpxe
MEDIA			+= elf
MEDIA			+= elfd
MEDIA			+= lmelf
MEDIA			+= lmelfd
MEDIA			+= lkrn
MEDIA			+= bImage
MEDIA			+= dsk
MEDIA			+= nbi
MEDIA			+= hd
MEDIA			+= raw
MEDIA			+= com
MEDIA			+= exe

# Special target for building Master Boot Record binary
$(BIN)/mbr.bin : $(BIN)/mbr.o
	$(OBJCOPY) -O binary $< $@

# Some suffixes (e.g. %.fd0) are generated directly from other
# finished files (e.g. %.dsk), rather than having their own prefix.

# rule to write disk images to /dev/fd0
NON_AUTO_MEDIA		+= fd0
%fd0 : %dsk
	dd if=$< bs=512 conv=sync of=/dev/fd0
	sync

# rule to create padded disk images
NON_AUTO_MEDIA		+= pdsk
%pdsk : %dsk
	cp $< $@
	$(PERL) ./util/dskpad.pl $@

# rule to make a non-emulation ISO boot image
NON_AUTO_MEDIA		+= iso
%iso:	%lkrn util/geniso
	ISOLINUX_BIN=$(ISOLINUX_BIN) bash util/geniso $@ $<

# rule to make a floppy emulation ISO boot image
NON_AUTO_MEDIA		+= liso
%liso:	%lkrn util/genliso
	bash util/genliso $@ $<

# rule to make a USB disk image
$(BIN)/usbdisk.bin : $(BIN)/usbdisk.o
	$(OBJCOPY) -O binary $< $@

NON_AUTO_MEDIA		+= usb
%usb: $(BIN)/usbdisk.bin %hd
	cat $^ > $@

# Add NON_AUTO_MEDIA to the media list, so that they show up in the
# output of "make"
#
MEDIA		+= $(NON_AUTO_MEDIA)

# Shortcut to allow typing just
#   make bin-kir/%
# rather than
#   make -f arch/i386/kir-Makefile bin-kir/%
# for building a KEEP_IT_REAL flavour.
#
$(BIN)-kir/% : kir-target
	$(MAKE) -f arch/i386/kir-Makefile $(MAKECMDGOALS)

.PHONY : kir-target
