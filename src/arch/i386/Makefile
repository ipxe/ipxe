ARCH_FORMAT=	elf32-i386

# For debugging, don't delete intermediates
#.SECONDARY:

LDSCRIPT=	arch/i386/core/etherboot.lds
PLDSCRIPT=	arch/i386/core/etherboot.prefix.lds

LCONFIG+=	-Ui386

ROMLIMIT=	524288
CHECKSIZE=	{ read d1; read d1 d2 d3 size d4; [ $$size -gt $(ROMLIMIT) ] &&\
	{ $(RM) $@; echo "ERROR: code size exceeds limit!"; exit 1; }; exit 0; }

START=	$(BIN)/start32.o $(BIN)/linuxbios.o \
	$(BIN)/bios.o $(BIN)/console.o $(BIN)/memsizes.o $(BIN)/basemem.o \
	$(BIN)/hidemem.o $(BIN)/e820mangler.o \
	$(BIN)/realmode.o $(BIN)/realmode_asm.o \
	$(BIN)/callbacks.o $(BIN)/pxe_callbacks.o

SRCS+=	arch/i386/prefix/floppyprefix.S
SRCS+=	arch/i386/prefix/unhuf.S
SRCS+=	arch/i386/prefix/unnrv2b.S
SRCS+=	arch/i386/firmware/pcbios/bios.c
SRCS+=	arch/i386/firmware/pcbios/console.c
SRCS+=	arch/i386/firmware/pcbios/memsizes.c
SRCS+=	arch/i386/firmware/pcbios/basemem.c
SRCS+=	arch/i386/firmware/pcbios/hidemem.c
SRCS+=	arch/i386/firmware/pcbios/e820mangler.S
SRCS+=	arch/i386/prefix/liloprefix.S
SRCS+=	arch/i386/prefix/elfprefix.S
SRCS+=	arch/i386/prefix/lmelf_prefix.S
SRCS+=	arch/i386/prefix/elf_dprefix.S
SRCS+=	arch/i386/prefix/lmelf_dprefix.S
SRCS+=	arch/i386/prefix/comprefix.S
SRCS+=	arch/i386/prefix/exeprefix.S
SRCS+=	arch/i386/prefix/pxeprefix.S
SRCS+=	arch/i386/prefix/romprefix.S

SRCS+=	arch/i386/core/init.S
SRCS+=	arch/i386/core/start32.S
SRCS+=	arch/i386/core/pci_io.c
SRCS+=	arch/i386/core/i386_timer.c
SRCS+=	arch/i386/core/elf.c
SRCS+=	arch/i386/core/cpu.c
SRCS+=	arch/i386/core/video_subr.c
SRCS+=	arch/i386/core/pic8259.c
SRCS+=	arch/i386/core/hooks.c
SRCS+=	arch/i386/core/callbacks.c
SRCS+=	arch/i386/core/realmode.c
SRCS+=	arch/i386/core/realmode_asm.S
SRCS+=	arch/i386/core/pxe_callbacks.c

# ROM loaders: ISA and PCI versions
ISAPREFIX=	$(BIN)/isaprefix.o
ISAENTRY=	$(BIN)/isaprefix.entry.o
ISAEXIT=	$(BIN)/isaprefix.exit.o
PCIPREFIX=	$(BIN)/pciprefix.o
PCIENTRY=	$(BIN)/pciprefix.entry.o
PCIEXIT=	$(BIN)/pciprefix.exit.o
# Variables xxx_ROMTYPE are defined by genrules.pl.  ROMENTRY and
# ROMEXIT will evaluate to give the correct objects to use.
TARGETBASE=$(patsubst $(BIN)/%,%,$(firstword $(subst ., ,$@)))
ROMCARD=$(firstword $(subst --, ,$(TARGETBASE)))
ROMTYPE=$(firstword $(ROMTYPE_$(ROMCARD)) ISA)
romENTRY=$($(ROMTYPE)ENTRY)
romEXIT=$($(ROMTYPE)EXIT)

# Target type for generic prf rules
TARGETTYPE=$(patsubst .%,%, $(suffix $(basename $@)))
TARGETENTRY=$($(TARGETTYPE)ENTRY)
TARGETEXIT=$($(TARGETTYPE)EXIT)

# Other real-mode entry loaders
dskPREFIX=	$(BIN)/floppyprefix.o
dskENTRY=	$(BIN)/floppyprefix.entry.o
dskEXIT=	$(BIN)/floppyprefix.exit.o
comPREFIX=	$(BIN)/comprefix.o
comENTRY=	$(BIN)/comprefix.entry.o
comEXIT=	$(BIN)/comprefix.exit.o
exePREFIX=	$(BIN)/exeprefix.o
exeENTRY=	$(BIN)/exeprefix.entry.o
exeEXIT=	$(BIN)/exeprefix.exit.o
liloPREFIX=	$(BIN)/liloprefix.o
liloENTRY=	$(BIN)/liloprefix.entry.o
liloEXIT=	$(BIN)/liloprefix.exit.o
bImagePREFIX=	$(BIN)/bImageprefix.o
bImageENTRY=	$(BIN)/bImageprefix.entry.o
bImageEXIT=	$(BIN)/bImageprefix.exit.o
pxePREFIX=	$(BIN)/pxeprefix.o
pxeENTRY=	$(BIN)/pxeprefix.entry.o
pxeEXIT=	$(BIN)/pxeprefix.exit.o
rawPREFIX=	$(BIN)/nullprefix.o
rawENTRY=	$(BIN)/nullprefix.entry.o
rawEXIT=	$(BIN)/nullprefix.exit.o

# Protected mode entry loaders
elfPREFIX=	$(BIN)/elfprefix.o
elfENTRY=	$(BIN)/elfprefix.entry.o
elfEXIT=	$(BIN)/elfprefix.exit.o
lmelfPREFIX=	$(BIN)/lmelf_prefix.o
lmelfENTRY=	$(BIN)/lmelf_prefix.entry.o
lmelfEXIT=	$(BIN)/lmelf_prefix.exit.o
elfdPREFIX=	$(BIN)/elf_dprefix.o
elfdENTRY=	$(BIN)/elf_dprefix.entry.o
elfdEXIT=	$(BIN)/elf_dprefix.exit.o
lmelfdPREFIX=	$(BIN)/lmelf_dprefix.o
lmelfdENTRY=	$(BIN)/lmelf_dprefix.entry.o
lmelfdEXIT=	$(BIN)/lmelf_dprefix.exit.o

include		$(BIN)/Roms

all:		$(ROMS)
allroms:	$(ROMS)
allzroms:	$(ROMS)
alldsks:	$(EB_DSKS)
allzdsks:	$(EB_ZDSKS)
alllilos:	$(EB_LILOS)
allzlilos:	$(EB_ZLILOS)
allbImages:	$(EB_BIMAGES)
allbzImages:	$(EB_BZIMAGES)
allpxes:	$(EB_PXES)
allzpxes:	$(EB_ZPXES)
allelfs:	$(EB_ELFS)
allzelfs:	$(EB_ZELFS)
alllmelfs:	$(EB_LMELFS)
allzlmelfs:	$(EB_ZLMELFS)
allelfds:	$(EB_ELFDS)
allzelfds:	$(EB_ZELFDS)
alllmelfds:	$(EB_LMELFDS)
allzlmelfds:	$(EB_ZLMELFDS)
allcoms:	$(EB_COMS)
allexes:	$(EB_EXES)
allisos:	$(EB_ISOS)
alllisos:	$(EB_LISOS)

BOBJS+=		$(BIN)/pci_io.o $(BIN)/i386_timer.o
BOBJS+=		$(BIN)/elf.o $(BIN)/cpu.o $(BIN)/video_subr.o
BOBJS+=		$(BIN)/pic8259.o $(BIN)/hooks.o

# ROM loaders

$(ISAPREFIX):	arch/i386/prefix/romprefix.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) $(LCONFIG) -Ui386 -D ASSEMBLY $< \
		| $(AS) $(ASFLAGS) -o $@ 

$(PCIPREFIX):	arch/i386/prefix/romprefix.S $(MAKEDEPS)
	$(CPP) -DPCI_PNP_HEADER $(CFLAGS) $(LCONFIG) -Ui386 -D ASSEMBLY $< \
		| $(AS) $(ASFLAGS) -o $@ 

# Prefix splitters
$(BIN)/%prefix.entry.o: $(BIN)/%prefix.o $(MAKEDEPS)
	$(OBJCOPY) -R .text16 $< $@

$(BIN)/%prefix.exit.o: $(BIN)/%prefix.o $(MAKEDEPS)
	$(OBJCOPY) -R .prefix $< $@

# Generic prefix objects
PREFIXOBJS = $(BIN)/init.o
ZPREFIXOBJS = $(BIN)/init.o $(BIN)/unnrv2b.o

# Utilities
$(BIN)/nrv2b:	util/nrv2b.c
	$(HOST_CC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<

ZFILELEN = perl util/zfilelen.pl

# Pattern Rules

# General for compiling/assembly source files

$(BIN)/%.o:	arch/i386/core/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/i386/core/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	arch/i386/firmware/pcbios/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/i386/firmware/pcbios/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	arch/i386/prefix/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

# general rule for 16bit .o, may be overridden
$(BIN)/%.o:	$(BIN)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# general rule for .bin (plain binary loader code), may be overridden
$(BIN)/%.bin:	$(BIN)/%.o
	$(OBJCOPY) -O binary $< $@

# general rule for .z (compressed binary code), may be overridden
# rule for .z is in top level Makefile
# Give the directory name, e.g. use $(BIN)/rtl8139.com as the target.

$(BIN)/%.zo:	$(BIN)/%.zbin arch/i386/core/prefixzdata.lds $(MAKEDEPS)
	$(LD) -T arch/i386/core/prefixzdata.lds -b binary $< -o $@

$(BIN)/%.uo:	$(BIN)/%.bin arch/i386/core/prefixudata.lds $(MAKEDEPS)
	$(LD) -T arch/i386/core/prefixudata.lds -b binary $< -o $@

# Intermediate prf rules

%.prf:  %.rt $(PREFIXOBJS) %.rt1.uo %.rt2.uo $(MAKEDEPS)
	$(MAKE) $(TARGETENTRY)
	$(LD) $(LDFLAGS) -T $(PLDSCRIPT) $(TARGETENTRY) -R $(subst $(MAKEDEPS),,$^)  -o $@ 

%.zprf:  %.rt $(ZPREFIXOBJS) %.rt1.uo %.rt2.zo $(MAKEDEPS)
	$(MAKE) $(TARGETENTRY)
	$(LD) $(LDFLAGS) -T $(PLDSCRIPT) $(TARGETENTRY) -R $(subst $(MAKEDEPS),,$^)  -o $@ 

# general rules for normal/compressed ROM images, may be overridden
SUFFIXES +=	rom zrom

$(BIN)/%.rom.rt: $(BIN)/%.rt.o  $(ISAENTRY) $(PCIENTRY) $(ISAEXIT) $(PCIEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(romEXIT) $<
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.rom: $(BIN)/%.rom.prf
	$(OBJCOPY) -O binary $< $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$(ROMCARD)) $(MAKEROM_ID_$(ROMCARD)) -i$(IDENT) $@

$(BIN)/%.zrom: $(BIN)/%.rom.zprf
	$(OBJCOPY) -O binary $< $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$(ROMCARD)) $(MAKEROM_ID_$(ROMCARD)) -i$(IDENT) $@

# general rules for ELF images
SUFFIXES +=	elf zelf
$(BIN)/%.elf.rt:  $(BIN)/%.rt.o $(elfENTRY) $(elfEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(elfEXIT)  $< 

$(BIN)/%.elf: $(BIN)/%.elf.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zelf: $(BIN)/%.elf.zprf 
	$(OBJCOPY) -O binary $< $@

# general rules for Long Mode ELF images
SUFFIXES +=	lmelf zlmelf
$(BIN)/%.lmelf.rt: $(BIN)/%.rt.o $(lmelfENTRY) $(lmelfEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(lmelfEXIT) $<

$(BIN)/%.lmelf: $(BIN)/%.lmelf.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zlmelf: $(BIN)/%.lmelf.zprf
	$(OBJCOPY) -O binary $< $@

# general rules for ELF dynamic images
SUFFIXES +=	elfd zelfd
$(BIN)/%.elfd.rt: $(BIN)/%.rt.o $(elfdENTRY) $(elfdEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(elfdEXIT) $<

$(BIN)/%.elfd: $(BIN)/%.elfd.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zelfd: $(BIN)/%.elfd.zprf
	$(OBJCOPY) -O binary $< $@

# general rules for Long Mode ELF dynamic images
SUFFIXES +=	lmelfd zlmelfd
$(BIN)/%.lmelfd.rt: $(BIN)/%.rt.o $(lmelfdENTRY) $(lmelfdEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(lmelfdEXIT) $<

$(BIN)/%.lmelfd: $(BIN)/%.lmelfd.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zlmelfd: $(BIN)/%.lmelfd.zprf
	$(OBJCOPY) -O binary $< $@

# rules to generate a DOS loadable .com executable
SUFFIXES +=	com
$(BIN)/%.com.rt: $(BIN)/%.rt.o $(comENTRY) $(comEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(comEXIT)

$(BIN)/%.com: $(BIN)/%.com.zprf
	$(OBJCOPY) -O binary $< $@

# rules to generate a DOS loadable .exe executable
SUFFIXES +=	exe
$(BIN)/%.exe.rt: $(BIN)/%.rt.o $(exeENTRY) $(exeEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(exeEXIT)
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.exe: $(BIN)/%.exe.prf
	$(OBJCOPY) -O binary $< $@

# rules to make a LILO loadable image
SUFFIXES +=	lilo zlilo

$(BIN)/%.lilo.rt: $(BIN)/%.rt.o $(liloENTRY) $(liloEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(liloEXIT)
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.lilo: $(BIN)/%.lilo.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zlilo: $(BIN)/%.lilo.zprf
	$(OBJCOPY) -O binary $< $@

# rules to make big linux boot protocol image
SUFFIXES +=	bImage bzImage

$(BIN)/%.bImage.rt: $(BIN)/%.rt.o $(bImageENTRY) $(bImageEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(bImageEXIT)

$(BIN)/%.bImage: $(BIN)/%.bImage.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.bzImage: $(BIN)/%.bImage.zprf
	$(OBJCOPY) -O binary $< $@


# rules to generate a PXE loadable image
SUFFIXES +=	pxe zpxe

$(BIN)/%.pxe.rt: $(BIN)/%.rt.o $(pxeENTRY) $(pxeEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(pxeEXIT)
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.pxe: $(BIN)/%.pxe.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zpxe: $(BIN)/%.pxe.zprf
	$(OBJCOPY) -O binary $< $@

# rules to generate the .dsk/.zdsk floppy images
SUFFIXES +=	dsk zdsk

$(BIN)/%.dsk.rt: $(BIN)/%.rt.o $(dskENTRY) $(dskEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(dskEXIT)
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.dsk: $(BIN)/%.dsk.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zdsk: $(BIN)/%.dsk.zprf
	$(OBJCOPY) -O binary $< $@

# rules to write the .dsk/.zdsk image onto a blank floppy
SUFFIXES +=	fd0 zfd0
%.fd0:	%.dsk
	dd if=$< bs=512 conv=sync of=/dev/fd0
	sync

%.zfd0:	%.zdsk
	dd if=$< bs=512 conv=sync of=/dev/fd0
	sync

# rules to create raw executable images
SUFFIXES +=	raw zraw
$(BIN)/%.raw.rt: $(BIN)/%.rt.o $(rawENTRY) $(rawEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $< $(rawEXIT)

$(BIN)/%.raw: $(BIN)/%.raw.prf
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.zraw: $(BIN)/%.raw.zprf
	$(OBJCOPY) -O binary $< $@

# rule to make a non-emulation ISO boot image
SUFFIXES +=	iso
%.iso:	util/geniso %.zlilo
	ISOLINUX_BIN=${ISOLINUX_BIN} bash util/geniso $*.iso $*.zlilo

# rule to make a floppy emulation ISO boot image
SUFFIXES +=	liso
%.liso:	util/genliso %.zlilo
	bash util/genliso $*.liso $*.zlilo

