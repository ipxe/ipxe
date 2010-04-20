/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER )

	.arch i386

/**
 * High memory temporary load address
 *
 * Temporary buffer into which to copy (or decompress) our runtime
 * image, prior to calling get_memmap() and relocate().  We don't
 * actually leave anything here once install() has returned.
 *
 * We use the start of an even megabyte so that we don't have to worry
 * about the current state of the A20 line.
 *
 * We use 4MB rather than 2MB because some PXE stack / PMM BIOS
 * combinations are known to place data required by other UNDI ROMs
 * loader around the 2MB mark.
 */
	.globl	HIGHMEM_LOADPOINT
	.equ	HIGHMEM_LOADPOINT, ( 4 << 20 )

/* Image compression enabled */
#define COMPRESS 1

/*****************************************************************************
 * Utility function: print character (with LF -> LF,CR translation)
 *
 * Parameters:
 *   %al : character to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	print_character
print_character:
	/* Preserve registers */
	pushw	%ax
	pushw	%bx
	pushw	%bp
	/* If %di is non-zero, write character to buffer and exit */
	testw	%di, %di
	jz	1f
	movb	%al, %ds:(%di)
	incw	%di
	jmp	3f
1:	/* Print character */
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	cmpb	$0x0a, %al		/* '\n'? */
	jne	2f
	int	$0x10
	movb	$0x0d, %al
2:	int	$0x10
	/* Restore registers and return */
3:	popw	%bp
	popw	%bx
	popw	%ax
	ret
	.size	print_character, . - print_character

/*****************************************************************************
 * Utility function: print a NUL-terminated string
 *
 * Parameters:
 *   %ds:si : string to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:si : character after terminating NUL
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	print_message
print_message:
	/* Preserve registers */
	pushw	%ax
	/* Print string */
1: 	lodsb
	testb	%al, %al
	je	2f
	call	print_character
	jmp	1b
2:	/* Restore registers and return */
	popw	%ax
	ret
	.size	print_message, . - print_message

/*****************************************************************************
 * Utility functions: print hex digit/byte/word/dword
 *
 * Parameters:
 *   %al (low nibble) : digit to print
 *   %al : byte to print
 *   %ax : word to print
 *   %eax : dword to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	print_hex_dword
print_hex_dword:
	rorl	$16, %eax
	call	print_hex_word
	rorl	$16, %eax
	/* Fall through */
	.size	print_hex_dword, . - print_hex_dword
	.globl	print_hex_word
print_hex_word:
	xchgb	%al, %ah
	call	print_hex_byte
	xchgb	%al, %ah
	/* Fall through */
	.size	print_hex_word, . - print_hex_word
	.globl	print_hex_byte
print_hex_byte:
	rorb	$4, %al
	call	print_hex_nibble
	rorb	$4, %al
	/* Fall through */
	.size	print_hex_byte, . - print_hex_byte
	.globl	print_hex_nibble
print_hex_nibble:
	/* Preserve registers */
	pushw	%ax
	/* Print digit (technique by Norbert Juffa <norbert.juffa@amd.com> */
	andb	$0x0f, %al
	cmpb	$10, %al
	sbbb	$0x69, %al
	das
	call	print_character
	/* Restore registers and return */
	popw	%ax
	ret
	.size	print_hex_nibble, . - print_hex_nibble

/*****************************************************************************
 * Utility function: print PCI bus:dev.fn
 *
 * Parameters:
 *   %ax : PCI bus:dev.fn to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	print_pci_busdevfn
print_pci_busdevfn:
	/* Preserve registers */
	pushw	%ax
	/* Print bus */
	xchgb	%al, %ah
	call	print_hex_byte
	/* Print ":" */
	movb	$( ':' ), %al
	call	print_character
	/* Print device */
	movb	%ah, %al
	shrb	$3, %al
	call	print_hex_byte
	/* Print "." */
	movb	$( '.' ), %al
	call	print_character
	/* Print function */
	movb	%ah, %al
	andb	$0x07, %al
	call	print_hex_nibble
	/* Restore registers and return */
	popw	%ax
	ret
	.size	print_pci_busdevfn, . - print_pci_busdevfn

/*****************************************************************************
 * Utility function: clear current line
 *
 * Parameters:
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	print_kill_line
print_kill_line:
	/* Preserve registers */
	pushw	%ax
	pushw	%cx
	/* Print CR */
	movb	$( '\r' ), %al
	call	print_character
	/* Print 79 spaces */
	movb	$( ' ' ), %al
	movw	$79, %cx
1:	call	print_character
	loop	1b
	/* Print CR */
	movb	$( '\r' ), %al
	call	print_character
	/* Restore registers and return */
	popw	%cx
	popw	%ax
	ret
	.size	print_kill_line, . - print_kill_line

/****************************************************************************
 * copy_bytes
 *
 * Copy bytes
 *
 * Parameters:
 *   %ds:esi : source address
 *   %es:edi : destination address
 *   %ecx : length
 * Returns:
 *   %ds:esi : next source address
 *   %es:edi : next destination address
 * Corrupts:
 *   None
 ****************************************************************************
 */
#if ! COMPRESS
	.section ".prefix.lib", "awx", @progbits
	.code16
copy_bytes:
	pushl %ecx
	rep addr32 movsb
	popl %ecx
	ret
	.size copy_bytes, . - copy_bytes
#endif /* COMPRESS */

/****************************************************************************
 * install_block
 *
 * Install block to specified address
 *
 * Parameters:
 *   %esi : source physical address (must be a multiple of 16)
 *   %edi : destination physical address (must be a multiple of 16)
 *   %ecx : length of (decompressed) data
 *   %edx : total length of block (including any uninitialised data portion)
 * Returns:
 *   %esi : next source physical address (will be a multiple of 16)
 *   %edi : next destination physical address (will be a multiple of 16)
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
install_block:
	/* Preserve registers */
	pushw	%ds
	pushw	%es
	pushl	%ecx
	
	/* Convert %esi and %edi to %ds:esi and %es:edi */
	shrl	$4, %esi
	movw	%si, %ds
	xorw	%si, %si
	shll	$4, %esi
	shrl	$4, %edi
	movw	%di, %es
	xorw	%di, %di
	shll	$4, %edi

#if COMPRESS
	/* Decompress source to destination */
	call	decompress16
#else
	/* Copy source to destination */
	call	copy_bytes
#endif

	/* Zero .bss portion */
	negl	%ecx
	addl	%edx, %ecx
	pushw	%ax
	xorw	%ax, %ax
	rep addr32 stosb
	popw	%ax

	/* Round up %esi and %edi to start of next blocks */
	addl	$0xf, %esi
	andl	$~0xf, %esi
	addl	$0xf, %edi
	andl	$~0xf, %edi

	/* Convert %ds:esi and %es:edi back to physical addresses */
	xorl	%ecx, %ecx
	movw	%ds, %cx
	shll	$4, %ecx
	addl	%ecx, %esi
	xorl	%ecx, %ecx
	movw	%es, %cx
	shll	$4, %ecx
	addl	%ecx, %edi

	/* Restore registers and return */
	popl	%ecx
	popw	%es
	popw	%ds
	ret
	.size install_block, . - install_block

/****************************************************************************
 * alloc_basemem
 *
 * Allocate space for .text16 and .data16 from top of base memory.
 * Memory is allocated using the BIOS free base memory counter at
 * 0x40:13.
 *
 * Parameters: 
 *   none
 * Returns:
 *   %ax : .text16 segment address
 *   %bx : .data16 segment address
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl	alloc_basemem
alloc_basemem:
	/* Preserve registers */
	pushw	%fs

	/* FBMS => %ax as segment address */
	pushw	$0x40
	popw	%fs
	movw	%fs:0x13, %ax
	shlw	$6, %ax

	/* Calculate .data16 segment address */
	subw	$_data16_memsz_pgh, %ax
	pushw	%ax

	/* Calculate .text16 segment address */
	subw	$_text16_memsz_pgh, %ax
	pushw	%ax

	/* Update FBMS */
	shrw	$6, %ax
	movw	%ax, %fs:0x13

	/* Retrieve .text16 and .data16 segment addresses */
	popw	%ax
	popw	%bx

	/* Restore registers and return */
	popw	%fs
	ret
	.size alloc_basemem, . - alloc_basemem

/****************************************************************************
 * free_basemem
 *
 * Free space allocated with alloc_basemem.
 *
 * Parameters:
 *   %ax : .text16 segment address
 *   %bx : .data16 segment address
 * Returns:
 *   %ax : 0 if successfully freed
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".text16", "ax", @progbits
	.code16
	.globl	free_basemem
free_basemem:
	/* Preserve registers */
	pushw	%fs

	/* Check FBMS counter */
	pushw	%ax
	shrw	$6, %ax
	pushw	$0x40
	popw	%fs
	cmpw	%ax, %fs:0x13
	popw	%ax
	jne	1f

	/* Check hooked interrupt count */
	cmpw	$0, %cs:hooked_bios_interrupts
	jne	1f

	/* OK to free memory */
	addw	$_text16_memsz_pgh, %ax
	addw	$_data16_memsz_pgh, %ax
	shrw	$6, %ax
	movw	%ax, %fs:0x13
	xorw	%ax, %ax

1:	/* Restore registers and return */
	popw	%fs
	ret
	.size free_basemem, . - free_basemem

	.section ".text16.data", "aw", @progbits
	.globl	hooked_bios_interrupts
hooked_bios_interrupts:
	.word	0
	.size	hooked_bios_interrupts, . - hooked_bios_interrupts

/****************************************************************************
 * install
 *
 * Install all text and data segments.
 *
 * Parameters:
 *   none
 * Returns:
 *   %ax  : .text16 segment address
 *   %bx  : .data16 segment address
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl install
install:
	/* Preserve registers */
	pushl	%esi
	pushl	%edi
	/* Allocate space for .text16 and .data16 */
	call	alloc_basemem
	/* Image source = %cs:0000 */
	xorl	%esi, %esi
	/* Image destination = HIGHMEM_LOADPOINT */
	movl	$HIGHMEM_LOADPOINT, %edi
	/* Install text and data segments */
	call	install_prealloc
	/* Restore registers and return */
	popl	%edi
	popl	%esi
	ret
	.size install, . - install

/****************************************************************************
 * install_prealloc
 *
 * Install all text and data segments.
 *
 * Parameters:
 *   %ax  : .text16 segment address
 *   %bx  : .data16 segment address
 *   %esi : Image source physical address (or zero for %cs:0000)
 *   %edi : Decompression temporary area physical address
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib", "awx", @progbits
	.code16
	.globl install_prealloc
install_prealloc:
	/* Save registers */
	pushal
	pushw	%ds
	pushw	%es

	/* Sanity: clear the direction flag asap */
	cld

	/* Copy decompression temporary area physical address to %ebp */
	movl	%edi, %ebp

	/* Install .text16.early */
	pushl	%esi
	xorl	%esi, %esi
	movw	%cs, %si
	shll	$4, %esi
	addl	$_text16_early_lma, %esi
	movzwl	%ax, %edi
	shll	$4, %edi
	movl	$_text16_early_filesz, %ecx
	movl	$_text16_early_memsz, %edx
	call	install_block		/* .text16.early */
	popl	%esi

	/* Open up access to payload */
#ifndef KEEP_IT_REAL
	/* Access high memory */
	pushw	%cs
	pushw	$1f
	pushw	%ax
	pushw	$access_highmem
	lret
1:	/* Die if we could not access high memory */
	jnc	3f
	movw	$a20_death_message, %si
	xorw	%di, %di
	call	print_message
2:	jmp	2b
	.section ".prefix.data", "aw", @progbits
a20_death_message:
	.asciz	"Gate A20 stuck - cannot continue\n"
	.size	a20_death_message, . - a20_death_message
	.previous
3:
#endif

	/* Calculate physical address of payload (i.e. first source) */
	testl	%esi, %esi
	jnz	1f
	movw	%cs, %si
	shll	$4, %esi
1:	addl	%cs:payload_lma, %esi

	/* Install .text16.late and .data16 */
	movl	$_text16_late_filesz, %ecx
	movl	$_text16_late_memsz, %edx
	call	install_block		/* .text16.late */
	movzwl	%bx, %edi
	shll	$4, %edi
	movl	$_data16_filesz, %ecx
	movl	$_data16_memsz, %edx
	call	install_block		/* .data16 */

	/* Set up %ds for access to .data16 */
	movw	%bx, %ds

#ifdef KEEP_IT_REAL
	/* Initialise libkir */
	movw	%ax, (init_libkir_vector+2)
	lcall	*init_libkir_vector
#else
	/* Install .text and .data to temporary area in high memory,
	 * prior to reading the E820 memory map and relocating
	 * properly.
	 */
	movl	%ebp, %edi
	movl	$_textdata_filesz, %ecx
	movl	$_textdata_memsz, %edx
	call	install_block

	/* Initialise librm at current location */
	movw	%ax, (init_librm_vector+2)
	movl	%ebp, %edi
	lcall	*init_librm_vector

	/* Call relocate() to determine target address for relocation.
	 * relocate() will return with %esi, %edi and %ecx set up
	 * ready for the copy to the new location.
	 */
	movw	%ax, (prot_call_vector+2)
	pushl	$relocate
	lcall	*prot_call_vector
	popl	%edx /* discard */

	/* Copy code to new location */
	pushl	%edi
	xorw	%ax, %ax
	movw	%ax, %es
	es rep addr32 movsb
	popl	%edi

	/* Initialise librm at new location */
	lcall	*init_librm_vector
#endif

	/* Restore registers */
	popw	%es
	popw	%ds
	popal
	ret
	.size install_prealloc, . - install_prealloc

	/* Vectors for far calls to .text16 functions.  Must be in
	 * .data16, since .prefix may not be writable.
	 */
	.section ".data16", "aw", @progbits
#ifdef KEEP_IT_REAL
init_libkir_vector:
	.word init_libkir
	.word 0
	.size init_libkir_vector, . - init_libkir_vector
#else
init_librm_vector:
	.word init_librm
	.word 0
	.size init_librm_vector, . - init_librm_vector
prot_call_vector:
	.word prot_call
	.word 0
	.size prot_call_vector, . - prot_call_vector
#endif

	/* Payload address */
	.section ".prefix.lib", "awx", @progbits
payload_lma:
	.long 0
	.section ".zinfo.fixup", "a", @progbits	/* Compressor fixups */
	.ascii	"ADHL"
	.long	payload_lma
	.long	1
	.long	0
	.previous

/****************************************************************************
 * uninstall
 *
 * Uninstall all text and data segments.
 *
 * Parameters:
 *   %ax  : .text16 segment address
 *   %bx  : .data16 segment address
 * Returns:
 *   none
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".text16", "ax", @progbits
	.code16
	.globl uninstall
uninstall:
	call	free_basemem
	ret
	.size uninstall, . - uninstall



	/* File split information for the compressor */
#if COMPRESS
#define PACK_OR_COPY	"PACK"
#else
#define PACK_OR_COPY	"COPY"
#endif
	.section ".zinfo", "a", @progbits
	.ascii	"COPY"
	.long	_prefix_lma
	.long	_prefix_filesz
	.long	_max_align
	.ascii	PACK_OR_COPY
	.long	_text16_early_lma
	.long	_text16_early_filesz
	.long	_max_align
	.ascii	"PAYL"
	.long	0
	.long	0
	.long	_max_align
	.ascii	PACK_OR_COPY
	.long	_text16_late_lma
	.long	_text16_late_filesz
	.long	_max_align
	.ascii	PACK_OR_COPY
	.long	_data16_lma
	.long	_data16_filesz
	.long	_max_align
	.ascii	PACK_OR_COPY
	.long	_textdata_lma
	.long	_textdata_filesz
	.long	_max_align
