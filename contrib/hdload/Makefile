# Use nasm or as86
ASM=nasm
# ASM=as86

hdload.bin:	hdload.S
ifeq ($(ASM),as86)
	gcc $(CFLAGS) -DUSE_AS86 -E -traditional -o hdload.s hdload.S
	as86 -0 -b hdload.bin hdload.s
else
	gcc $(CFLAGS) -DUSE_NASM -E -traditional -o hdload.s hdload.S
	nasm -f bin hdload.s -o hdload.bin
endif

clean:
	$(RM) -f hdload.s hdload.bin
