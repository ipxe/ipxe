ARCH_FORMAT=	coff-e1

BUILD_ROMS=	$(ROMS)
BUILD_COFFS=	$(patsubst %img, %coff, $(IMGS))
SUFFIXES+=	rom zrom coff

CC=		e1-coff-gcc
AS=		e1-coff-as
LD=		e1-coff-ld
SIZE=	e1-coff-size
AR=		e1-coff-ar
RANLIB=	e1-coff-ranlib
OBJCOPY=e1-coff-objcopy

# DMAC_HW_ADDR_DRV holds the ethernet's MAC address. It is passed as
# flag to the low level driver instead of reading it from an 
# external EEPROM, which we do not have!
EXTRA_CFLAGS = -DEMBEDDED -DMAC_HW_ADDR_DRV="'H','Y','L','N','X','1'"

START=	$(BIN)/start.o
START16=	$(BIN)/start.o

SRCS+=	arch/e1/core/e132_xs.c
SRCS+=	arch/e1/core/e1_timer.c
SRCS+=	arch/e1/core/longjmp.c
SRCS+=	arch/e1/core/memcmp.S
SRCS+=	arch/e1/core/memcpy.S
SRCS+=	arch/e1/core/memset.S
SRCS+=	arch/e1/core/setjmp.c
SRCS+=	arch/e1/core/strcmp.S
SRCS+=	arch/e1/core/start.S

ROMLIMIT:=3276800

include		$(BIN)/Roms

ROMS= $(BIN)/cs89x0.rom
IMGS= $(BIN)/cs89x0.img

#allfiles:	$(BUILD_ROMS)
all:	$(BUILD_COFFS)

BOBJS+=		$(BIN)/e1_timer.o
BOBJS+=		$(BIN)/memcmp.o $(BIN)/memcpy.o $(BIN)/memset.o
BOBJS+=		$(BIN)/setjmp.o $(BIN)/longjmp.o 
BOBJS+=		$(BIN)/e132_xs.o

# Utilities

$(BIN)/nrv2b:	util/nrv2b.c
	$(HOST_CC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<

# Pattern Rules
# General for compiling/assembly source files
$(BIN)/cs89x0.o:	drivers/net/cs89x0.c $(MAKEDEPS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
# With the current tools we have problem with the compilation 
# of the vsprintf file when the -O2 is selected. So we compile 
# the aforemntioned file with -O1 !!!
$(BIN)/vsprintf.o:	core/vsprintf.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -O1 -o $@ -c $<

$(BIN)/%.o:	arch/e1/core/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/e1/core/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.coff:	$(BIN)/%.tmp $(MAKEDEPS)
	mv $< $(BIN)/etherboot.coff
