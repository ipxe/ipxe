#include "elf.h"

	.arch i386
	.section ".prefix", "a", @progbits

#define LOAD_ADDR 0x10000

	/* ELF Header */
	.globl elf_header
elf_header:	
e_ident:	.byte	0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
e_type:		.short	ET_DYN
e_machine:	.short	EM_386
e_version:	.long	1
e_entry:	.long	LOAD_ADDR + _start - elf_header
e_phoff:	.long	elf_program_header - elf_header
e_shoff:	.long	0
e_flags:	.long	0
e_ehsize:	.short	elf_header_end - elf_header
e_phentsize:	.short	ELF32_PHDR_SIZE
e_phnum:	.short	(elf_program_header_end - elf_program_header)/ELF32_PHDR_SIZE
e_shentsize:	.short	0
e_shnum:	.short	0
e_shstrndx:	.short	0
elf_header_end:
	
elf_program_header:
phdr1_p_type:	.long	PT_NOTE
phdr1_p_offset:	.long	elf_note - elf_header
phdr1_p_vaddr:	.long	elf_note
phdr1_p_paddr:	.long	elf_note
phdr1_p_filesz:	.long	elf_note_end - elf_note
phdr1_p_memsz:	.long	elf_note_end - elf_note
phdr1_p_flags:	.long	PF_R | PF_W | PF_X
phdr1_p_align:	.long	0

/* The decompressor */
phdr2_p_type:	.long	PT_LOAD
phdr2_p_offset:	.long	0
phdr2_p_vaddr:	.long	elf_header
phdr2_p_paddr:	.long	LOAD_ADDR
phdr2_p_filesz:	.long	_verbatim_size
phdr2_p_memsz:	.long	_image_size
phdr2_p_flags:	.long	PF_R | PF_W | PF_X
phdr2_p_align:	.long	16

elf_program_header_end:

	.globl elf_note
elf_note:
	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_NAME
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.asciz "Etherboot"
4:


	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_VERSION
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.asciz VERSION
4:

#if 0
	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_CHECKSUM
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.word 0
4:
#endif
	.balign 4
elf_note_end:

	/* Dummy routines to satisfy the build */
	.section ".text16", "ax", @progbits
prefix_exit:

prefix_exit_end:
	.previous
