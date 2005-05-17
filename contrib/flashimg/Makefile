CPPFLAGS	= -x assembler-with-cpp
AS86		= as86
LD86		= ld86
OBJDUMP		= objdump

.SUFFIXES:	.s86 .asm .aout .img

all:	flashimg.img

clean:
	rm -rf *.o *.s86 *.aout *.img

realclean: clean
	rm -rf *.img

.asm.s86: $*.asm $*.inc
	  $(CPP) $(CPPFLAGS) -o $@ $*.asm

.s86.img: $*.s86
	  $(AS86) -0 -b $@ $*.s86

# .s86.o:	$*.s86
# 	  $(AS86) -0 -a -o $@ $*.s86
# 
# .o.aout: $*.o
# 	  $(LD86) -0 -s -o $@ $*.o
# 
# .aout.img:
# 	  dd if=$*.aout of=$@ bs=32 skip=1
