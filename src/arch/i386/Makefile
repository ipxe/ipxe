# Locations of utilities
#
ISOLINUX_BIN	= /usr/lib/syslinux/isolinux.bin

# i386-specific directories containing source files
#
SRCDIRS		+= arch/i386/core arch/i386/transitions arch/i386/prefix
SRCDIRS		+= arch/i386/firmware/pcbios
SRCDIRS		+= arch/i386/image
SRCDIRS		+= arch/i386/drivers/bus
SRCDIRS		+= arch/i386/drivers/net
SRCDIRS		+= arch/i386/drivers/disk
SRCDIRS		+= arch/i386/interface/pcbios
SRCDIRS		+= arch/i386/interface/pxe

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

# We need to undefine the default macro "i386" when compiling .S
# files, otherwise ".arch i386" translates to ".arch 1"...
#
CFLAGS_S		+= -Ui386

# The i386 linker script
#
LDSCRIPT		= arch/i386/scripts/i386.lds

# Media types.
# 
# It's ugly that we have to define these repetitive combinations by
# hand.  Unforunately, $(eval ...) is available only in make >= 3.80,
# and using an external Makefile fragment doesn't work because
# OBJS_xxx need to be defined *before* the external Makefile fragments
# for the source files are generated...

CFLAGS_ZPREFIX		= -DCOMPRESS

MEDIA			+= rom
OBJS_romprefix		= romprefix zromprefix
CFLAGS_zromprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= pxe
OBJS_pxeprefix		= pxeprefix zpxeprefix
CFLAGS_zpxeprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= elf
OBJS_elfprefix		= elfprefix zelfprefix
CFLAGS_zelfprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= elfd
OBJS_elfdprefix		= elfdprefix zelfdprefix
CFLAGS_zelfdprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= lmelf
OBJS_lmelfprefix	= lmelfprefix zlmelfprefix
CFLAGS_zlmelfprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= lmelfd
OBJS_lmelfdprefix	= lmelfdprefix zlmelfdprefix
CFLAGS_zlmelfdprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= lkrn
OBJS_lkrnprefix		= lkrnprefix zlkrnprefix
CFLAGS_zlkrnprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= bImage
OBJS_bImageprefix	= bImageprefix zbImageprefix
CFLAGS_zbImageprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= dsk
OBJS_dskprefix		= dskprefix zdskprefix
CFLAGS_zdskprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= nbi
OBJS_nbiprefix		= nbiprefix znbiprefix
CFLAGS_znbiprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= hd
OBJS_hdprefix		= hdprefix zhdprefix
CFLAGS_zhdprefix	= $(CFLAGS_ZPREFIX)

MEDIA			+= raw
OBJS_rawprefix		= rawprefix zrawprefix
CFLAGS_zrawprefix	= $(CFLAGS_ZPREFIX)

# These media cannot handle compressed payloads

MEDIA			+= com

MEDIA			+= exe

# Special target for building Master Boot Record binary
$(BIN)/mbr.bin : $(BIN)/mbr.o
	$(OBJCOPY) -O binary $< $@

# Some suffixes (e.g. %.zfd0) are generated directly from other
# finished files (e.g. %.zdsk), rather than having their own prefix.

# rule to write disk images to /dev/fd0
NON_AUTO_MEDIA		+= fd0
%fd0 : %dsk
	dd if=$< bs=512 conv=sync of=/dev/fd0
	sync

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
