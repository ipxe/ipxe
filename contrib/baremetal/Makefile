#
# Makefile for Etherboot
#
# Most of the time you should edit Config
#
# Common options:
#	VERSION=v	- Set the version string
#
# NS8390 options:
#	-DINCLUDE_NE	- Include NE1000/NE2000 support
#	-DNE_SCAN=list	- Probe for NE base address using list of
#			  comma separated hex addresses
#	-DINCLUDE_3C503 - Include 3c503 support
#	  -DT503_SHMEM	- Use 3c503 shared memory mode (off by default)
#	-DINCLUDE_WD	- Include Western Digital/SMC support
#	-DWD_DEFAULT_MEM- Default memory location for WD/SMC cards
#	-DCOMPEX_RL2000_FIX
#
#	If you have a Compex RL2000 PCI 32-bit (11F6:1401),
#	and the bootrom hangs in "Probing...[NE*000/PCI]",
#	try enabling this fix... it worked for me :).
#	In the first packet write somehow it somehow doesn't
#	get back the expected data so it is stuck in a loop.
#	I didn't bother to investigate what or why because it works
#	when I interrupt the loop if it takes more then COMPEX_RL2000_TRIES.
#	The code will notify if it does a abort.
#	SomniOne - somnione@gmx.net
#
# 3C509 option:
#	-DINCLUDE_3C509	- Include 3c509 support
#
# 3C90X options:
#	-DINCLUDE_3C90X	- Include 3c90x support
#	-DCFG_3C90X_PRESERVE_XCVR - Reset the transceiver type to the value it
#			  had initially just before the loaded code is started.
#	-DCFG_3C90X_XCVR - Hardcode the tranceiver type Etherboot uses.
#	-DCFG_3C90X_BOOTROM_FIX - If you have a 3c905B with buggy ROM
#			  interface, setting this option might "fix" it.  Use
#			  with caution and read the docs in 3c90x.txt!
#
#	See the documentation file 3c90x.txt for more details.
#
# CS89X0 (optional) options:
#	-DINCLUDE_CS89X0- Include CS89x0 support
#	-DCS_SCAN=list	- Probe for CS89x0 base address using list of
#			  comma separated hex addresses; increasing the
#			  address by one (0x300 -> 0x301) will force a
#			  more aggressive probing algorithm. This might
#			  be neccessary after a soft-reset of the NIC.
#
# LANCE options:
#	-DINCLUDE_NE2100- Include NE2100 support
#	-DINCLUDE_NI6510- Include NI6510 support
#
# SK_G16 options:
#	-DINCLUDE_SK_G16- Include SK_G16 support
#
# I82586 options:
#	-DINCLUDE_3C507	- Include 3c507 support
#	-DINCLUDE_NI5210- Include NI5210 support
#	-DINCLUDE_EXOS205-Include EXOS205 support
#
# SMC9000 options:
#       -DINCLUDE_SMC9000   - Include SMC9000 driver
#       -DSMC9000_SCAN=list - List of I/O addresses to probe
#
# TIARA (Fujitsu Etherstar) options:
#	-DINCLUDE_TIARA	- Include Tiara support
#
# NI5010 options:
#	-DINCLUDE_NI5010 - Include NI5010 support
#
# TULIP options:
#	-DINCLUDE_TULIP	- Include Tulip support
#	-DUSE_INTERNAL_BUFFER - receuve and transmit buffers within program
#		space, not below 0x10000, in case that region is used
#
# RTL8139 options:
#	-DINCLUDE_RTL8139 - Include RTL8139 support
#	-DUSE_INTERNAL_BUFFER - 8 kB receive buffer within program space,
#		not at 0x10000 - 8kB, in case that region is used
#

include Config

GCC=		gcc
CPP=		gcc -E
VERSION=	4.6.12
CFLAGS16+=	-DVERSION=\"$(VERSION)\" -DRELOC=$(RELOCADDR)
CFLAGS32+=	-DVERSION=\"$(VERSION)\" -DRELOC=$(RELOCADDR) $(OLDGAS)
LCONFIG+=	-DRELOC=$(RELOCADDR)

IDENT16=		'Etherboot/16 $(VERSION) (GPL) $(@F)'
IDENT32=		'Etherboot/32 $(VERSION) (GPL) $(@F)'

# Find out if we're using binutils 2.9.1 which uses a different syntax in some
# places (most prominently in the opcode prefix area).
OLDGAS:=	$(shell $(AS) --version | grep -q '2\.9\.1' && echo -DGAS291)

# Check the requested type of build (32, 16 or both families)
ifeq ($(ETHERBOOT),16)
BUILD_LIBS=	$(BLIB16)
BUILD_BINS=	$(BINS16)
endif
ifeq ($(ETHERBOOT),32)
BUILD_LIBS=	$(BLIB32)
BUILD_BINS=	$(BINS32)
endif
ifeq ($(ETHERBOOT),both)
BUILD_LIBS=	$(BLIB16) $(BLIB32)
BUILD_BINS=	$(BINS16) $(BINS32)
endif

3C503FLAGS=	-DINCLUDE_3C503 # -DT503_SHMEM
# Note that the suffix to MAKEROM_ is the (mixed case) basename of the ROM file
MAKEROM_3c503=	-3
3C507FLAGS=	-DINCLUDE_3C507
3C509FLAGS=	-DINCLUDE_3C509
3C529FLAGS=	-DINCLUDE_3C529
3C595FLAGS=	-DINCLUDE_3C595
3C90XFLAGS=	-DINCLUDE_3C90X
CS89X0FLAGS=	-DINCLUDE_CS89X0
EEPROFLAGS=	-DINCLUDE_EEPRO
EEPRO100FLAGS=	-DINCLUDE_EEPRO100
EPIC100FLAGS=	-DINCLUDE_EPIC100
EXOS205FLAGS=	-DINCLUDE_EXOS205
LANCEFLAGS=	-DINCLUDE_LANCE		# Lance/PCI!
NE2100FLAGS=	-DINCLUDE_NE2100
NEFLAGS=	-DINCLUDE_NE -DNE_SCAN=0x300,0x280,0x320,0x340,0x380
NS8390FLAGS=	-DINCLUDE_NS8390	# NE2000/PCI!
NI5010FLAGS=	-DINCLUDE_NI5010
NI5210FLAGS=	-DINCLUDE_NI5210
NI6510FLAGS=	-DINCLUDE_NI6510
RTL8139FLAGS=	-DINCLUDE_RTL8139
SK_G16FLAGS=	-DINCLUDE_SK_G16
SMC9000FLAGS=   -DINCLUDE_SMC9000
TIARAFLAGS=	-DINCLUDE_TIARA
DEPCAFLAGS=	-DINCLUDE_DEPCA	# -DDEPCA_MODEL=DEPCA -DDEPCA_RAM_BASE=0xd0000
TULIPFLAGS=	-DINCLUDE_TULIP
OTULIPFLAGS=	-DINCLUDE_OTULIP
VIA_RHINEFLAGS=	-DINCLUDE_VIA_RHINE
WDFLAGS=	-DINCLUDE_WD -DWD_DEFAULT_MEM=0xCC000
W89C840FLAGS=	-DINCLUDE_W89C840

# If you have not made any changes to the *.S files, AS86 need not be set.
# (most people)
# If you have made changes to the *.S files and you want to rebuild *loader.bin
# and {floppy,com}load.bin and you have as86 from the ELKS Dev86 package (not
# the one that normally comes with Linux) (not most people)
#AS86=		as86
# If you have made changes to the *.S files and you want to rebuild *loader.bin
# and {floppy,com}load.bin and you have nasm (not most people)
#AS86=		nasm

# if your as has trouble with the data32 directive, uncomment this
# but note that the premade start*.o will be larger than necessary because it
# contains some routines which may not be used
#AS_PSEUDOS=	n

SRCS=	floppyload.S comload.S liloprefix.S loader.S start16.S start32.S serial.S startmpcc.S
SRCS+=	main.c pci.c osloader.c nfs.c misc.c ansiesc.c bootmenu.c config.c
SRCS+=	md5.c floppy.c

# ROM loaders: LZ version (prefix Z), PCI header version (prefix P)
ifndef AS86
RLOADER=	rloader.bin.pre
PRLOADER=	prloader.bin.pre
RZLOADER=	rzloader.bin.pre
PRZLOADER=	przloader.bin.pre
FLOPPYLOAD=	floppyload.bin.pre
COMLOAD=	comload.bin.pre
LILOPREFIX=	liloprefix.bin.pre
else
RLOADER=	bin/rloader.bin
PRLOADER=	bin/prloader.bin
RZLOADER=	bin/rzloader.bin
PRZLOADER=	bin/przloader.bin
FLOPPYLOAD=	bin/floppyload.bin
COMLOAD=	bin/comload.bin
LILOPREFIX=	bin/liloprefix.bin
endif

ifeq ($(AS86),as86)
LCPPFLAGS+=	-DUSE_AS86
LASFLAGS+=	$(AS86FLAGS) -0
LASBINARY:=	-b
endif
ifeq ($(AS86),nasm)
LCPPFLAGS+=	-DUSE_NASM
LASFLAGS+=	$(NASMFLAGS) -fbin
LASBINARY:=	-o
endif

ifeq ($(AS_PSEUDOS),n)
START16=	start16.o.pre
START32=	start32.o.pre
else
START16=	bin16/start16.o
START32=	bin32/startmpcc.o
endif

BOBJS16=	bin16/main.o bin16/osloader.o bin16/misc.o bin16/bootmenu.o
BOBJS16+=	bin16/floppy.o bin16/timer.o
BOBJS32=	bin32/main.o bin32/osloader.o bin32/nfs.o bin32/misc.o
BOBJS32+=	bin32/ansiesc.o bin32/bootmenu.o bin32/md5.o bin32/floppy.o
BOBJS32+=	bin32/serial.o bin32/timer.o
BLIB16=	bin16/bootlib.a
BLIB32=	bin32/bootlib.a
LIBS16=	$(BLIB16) $(LIBC16)
LIBS32=	$(BLIB32) $(LIBC32) /usr/lib/gcc-lib/i386-redhat-linux/2.96/libgcc.a
UTIL_LZHUF:= $(shell if [ -d ../contrib/compressor ]; then echo bin/lzhuf; fi)
UTILS+=	bin/makerom $(UTIL_LZHUF) bin/organon
STDDEPS16=	$(START16) $(BLIB16) $(UTILS)
STDDEPS32=	$(START32) $(BLIB32) $(UTILS)
MAKEDEPS=	Makefile Config Roms

CHECKSIZE=	{ read d1; read d1 d2 d3 size d4; [ $$size -gt $(ROMLIMIT) ] &&\
	{ $(RM) $@; echo "ERROR: code size exceeds limit!"; exit 1; }; exit 0; }

# Make sure that the relocation address is acceptable for all ROM sizes.
# Setting it to 0x98000 leaves about 29kB of space for the Etherboot program.
# The check is done based running 'size' on the binary, not ROM size, but
# roughly this means a ROM of 16kB or a partially used ROM of 32kB,
# remembering to compressed ROM images into account.
# You may also set RELOCADDR to 0x88000 to avoid using 0x98000
# because of other drivers (e.g. Disk On Chip). In that case, you may
# only load 512kB of OS, or load in memory above 1MB.
# Don't forget to choose an assembler because the loaders have to be rebuilt.
ifndef RELOCADDR
RELOCADDR=0x98000
#RELOCADDR=0xe0000
endif

# Evaluate ROMLIMIT only once - it is constant during the make run.
# Note that the 3K safety margin below is for the 1K extended BIOS data area
# and for the Etherboot runtime stack.  Under normal situations, 2K of stack
# are rarely needed.  If you experience strange behaviour in functions that use
# many local variables or that call functions that do, check for stack overrun!
# Make sure that the normal case needs no perl interpreter - if someone uses a
# different RELOCADDR, then he has perl installed anyways (the shell cannot
# deal with hex numbers, as test/eval don't support non-decimal integers).
ifeq ($(RELOCADDR),0x98000)
ROMLIMIT=29696
else
ROMLIMIT:=$(shell perl -e 'print 0x10000 - 3072 - ($(RELOCADDR) & 0xFFFF), "\n";')
endif

# Start of targets

all:	$(UTILS) $(BUILD_LIBS) allbins

include Roms

# We need allbins because $(BINS16) and $(BINS32) are not defined until
# the Makefile fragment "Roms" is read.

allbins:	$(BUILD_BINS)

# Common files

$(BLIB16):	$(BOBJS16)
	$(AR16) rv $@ $(BOBJS16)
	$(RANLIB16) $@

$(BLIB32):	$(BOBJS32)
	$(AR32) rv $@ $(BOBJS32)
	$(RANLIB32) $@

bin16/main.o:		main.c etherboot.h osdep.h nic.h
bin32/main.o:		main.c etherboot.h osdep.h nic.h

bin16/osloader.o:	osloader.c etherboot.h osdep.h
bin32/osloader.o:	osloader.c etherboot.h osdep.h

# NFS currently makes no sense for Etherboot/16
bin32/nfs.o:		nfs.c etherboot.h osdep.h nic.h

bin16/misc.o:		misc.c etherboot.h osdep.h
bin32/misc.o:		misc.c etherboot.h osdep.h

# ANSIESC is not supported for Etherboot/16
bin32/ansiesc.o:	ansiesc.c etherboot.h osdep.h

bin16/bootmenu.o:	bootmenu.c etherboot.h osdep.h
bin32/bootmenu.o:	bootmenu.c etherboot.h osdep.h

# Password support is not available for Etherboot/16
bin32/md5.o:		md5.c etherboot.h osdep.h

bin16/floppy.o:		floppy.c etherboot.h osdep.h
bin32/floppy.o:		floppy.c etherboot.h osdep.h

bin16/timer.o:		timer.c timer.h etherboot.h osdep.h
bin32/timer.o:		timer.c timer.h etherboot.h osdep.h

bin32/inthw.o:		inthw.c

# PCI support code (common to all PCI drivers)

bin32/pci.o:	pci.c pci.h

# Do not add driver specific dependencies here unless it's something the
# genrules.pl script *can't* deal with, i.e. if it is not C code.

# Prepended loaders

#ifndef AS86
#$(RLOADER) $(RZLOADER) $(PRLOADER) $(PRZLOADER):	$(MAKEDEPS)
#	@if [ $(RELOCADDR) != 0x98000 ]; then echo Non-standard RELOCADDR, must assemble $@; exit 1; fi
#	$(TOUCH) $@
#else
#bin/rloader.s:	loader.S $(MAKEDEPS)
#	$(CPP) $(LCPPFLAGS) $(LCONFIG) -o $@ $<
#
#bin/rzloader.s:	loader.S $(MAKEDEPS)
#	$(CPP) $(LCPPFLAGS) $(LCONFIG) -DZLOADER -o $@ $<
#
#bin/prloader.s:	loader.S $(MAKEDEPS)
#	$(CPP) $(LCPPFLAGS) $(LCONFIG) -DPCI_PNP_HEADER -o $@ $<
#
#bin/przloader.s:	loader.S $(MAKEDEPS)
#	$(CPP) $(LCPPFLAGS) $(LCONFIG) -DPCI_PNP_HEADER -DZLOADER -o $@ $<
#endif

# Floppy loader

ifdef AS86
bin/floppyload.s:	floppyload.S $(MAKEDEPS)
	$(CPP) $(LCPPFLAGS) -o $@ $<
endif

# COM loader

ifdef AS86
bin/comload.s:	comload.S $(MAKEDEPS)
	$(CPP) $(LCPPFLAGS) -o $@ $<
endif

# LILO prefix:

ifdef AS86
bin/liloprefix.s:	liloprefix.S $(MAKEDEPS)
	$(CPP) $(LCPPFLAGS) -o $@ $<
endif

# Utilities

bin/makerom: makerom.c
	$(GCC) -O2 -o $@ makerom.c

bin/organon: organon.c
	$(GCC) -o $@ organon.c

bin/lzhuf:	../contrib/compressor/lzhuf.c
	$(GCC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -o $@ $<

# Roms file

Roms:	NIC genrules.pl
	@chmod +x genrules.pl
	./genrules.pl NIC > $@

# Pattern Rules

# general rules for compiling/assembling source files
bin16/%.o:	%.c $(MAKEDEPS)
	$(CC16) $(CFLAGS16) -o $@ -c $<

bin32/%.o:	%.c $(MAKEDEPS)
	$(CC32) $(CFLAGS32) -o $@ -c $<

bin16/%.o:	%.S $(MAKEDEPS)
	$(CC16) $(CFLAGS16) $(ASFLAGS16) -c -o $@ $<

bin32/%.o:	%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS32) $< | $(AS) $(ASFLAGS32) -o $@

# general rule for .bin (plain binary loader code), may be overridden
ifdef AS86
bin/%.bin:	bin/%.s
	$(AS86) $(LASFLAGS) $(LASBINARY) $@ $<
endif

# general rule for .huf (compressed binary code), may be overridden
%.huf:	%.img
	bin/lzhuf e $< $@

# general rules for normal/compressed ROM images, may be overridden
bin16/%.rom:	bin16/%.img $(RLOADER)
	cat $(RLOADER) $< > $@
	bin/makerom $(MAKEROM_$*) -i$(IDENT16) $@

bin32/%.rom:	bin32/%.img $(RLOADER)
	cat $(RLOADER) $< > $@
	bin/makerom $(MAKEROM_$*) -i$(IDENT32) $@

bin16/%.lzrom:	bin16/%.huf $(RZLOADER)
	cat $(RZLOADER) $< > $@
	bin/makerom $(MAKEROM_$*) -i$(IDENT16) $@

bin32/%.lzrom:	bin32/%.huf $(RZLOADER)
	cat $(RZLOADER) $< > $@
	bin/makerom $(MAKEROM_$*) -i$(IDENT32) $@

# rules to write the .rom/.lzrom image onto a blank floppy
# You must give the directory name, e.g. use bin32/rtl8139.lzfd0 as the target.
%.fd0:	%.rom $(FLOPPYLOAD)
	cat $(FLOPPYLOAD) $< > /dev/fd0

%.lzfd0:	%.lzrom $(FLOPPYLOAD)
	cat $(FLOPPYLOAD) $< > /dev/fd0

# rules to generate a .com executable
# You must give the directory name, e.g. use bin32/rtl8139.com as the target.
%.com:	%.lzrom $(COMLOAD)
	cat $(COMLOAD) $< > $@

# rules to make a floppy image (padding to fill an even number of cylinders).
# VMware reports floppy image read errors if it cannot read ahead 36 sectors,
# probably because the floppyload.S code reads up to that number of sectors in
# a single request.  Not that 18k matters much these days...
# You must give the directory name, e.g. use bin32/rtl8139.fdimg as the target.
%.fdimg:	%.rom $(FLOPPYLOAD)
	cat $(FLOPPYLOAD) $< > $@.x
	dd if=$@.x of=$@ bs=36k conv=sync 2> /dev/null
	$(RM) $@.x

%.lzfdimg:	%.lzrom $(FLOPPYLOAD)
	cat $(FLOPPYLOAD) $< > $@.x
	dd if=$@.x of=$@ bs=36k conv=sync 2> /dev/null
	$(RM) $@.x

# rules to make a LILO-bootable image
%.lilo:		%.rom $(LILOPREFIX)
	cat $(LILOPREFIX) $< /dev/zero | head -c 64k > $@

%.lzlilo:	%.lzrom $(LILOPREFIX)
	cat $(LILOPREFIX) $< /dev/zero | head -c 64k > $@

# Housekeeping

# To make sure that this actually builds a start32.o.pre with all options set,
# you have to make sure that -DFLOPPY -DANSIESC -DCONSOLE_DUAL are in CFLAGS32.
precompiled:	bin/rloader.bin bin/rzloader.bin bin/prloader.bin bin/przloader.bin bin/floppyload.bin bin/comload.bin bin16/start16.o bin32/start32.o bin/liloprefix.bin
	cp -p bin/rloader.bin rloader.bin.pre
	cp -p bin/rzloader.bin rzloader.bin.pre
	cp -p bin/prloader.bin prloader.bin.pre
	cp -p bin/przloader.bin przloader.bin.pre
	cp -p bin/floppyload.bin floppyload.bin.pre
	cp -p bin/comload.bin comload.bin.pre
	cp -p bin16/start16.o start16.o.pre
	cp -p bin32/start32.o start32.o.pre
	cp -p bin/liloprefix.bin liloprefix.bin.pre

clean:
	$(RM) $(UTILS) bin/*.s bin/*.bin
	$(RM) $(BLIB16) $(BLIB32)
	$(RM) bin16/*.o bin32/*.o bin16/*.tmp bin32/*.tmp
	$(RM) bin16/*.img bin32/*.img bin16/*.huf bin32/*.huf
	$(RM) bin16/*.rom bin32/*.rom bin16/*.lzrom bin32/*.lzrom
	$(RM) bin16/*.com bin32/*.com
	$(RM) bin16/*.fdimg bin32/*.fdimg bin16/*.lzfdimg bin32/*.lzfdimg
	$(RM) bin16/*.lilo bin32/*.lilo bin16/*.lzlilo bin32/*.lzlilo
	$(RM) bin32/*.hex
	$(RM) bin32/*.asm
	$(RM) bin32/*.map

tarball:
	(echo -n $(VERSION) ''; date -u +'%Y-%m-%d') > ../VERSION
	(cd ..; tar cf /tmp/mpccboot-$(VERSION).tar --exclude CVS mpccboot)
	bzip2 -9 < /tmp/mpccboot-$(VERSION).tar > /tmp/mpccboot-$(VERSION).tar.bz2
	gzip -9 < /tmp/mpccboot-$(VERSION).tar > /tmp/mpccboot-$(VERSION).tar.gz

version:
	@echo $(VERSION)
