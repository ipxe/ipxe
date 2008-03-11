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

	.arch i386
	.section ".prefix.lib", "awx", @progbits
	.section ".data16", "aw", @progbits

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

#define CR0_PE 1

/*****************************************************************************
 * Utility function: print character (with LF -> LF,CR translation)
 *
 * Parameters:
 *   %al : character to print
 * Returns:
 *   Nothing
 * Corrupts:
 *   %ax
 *****************************************************************************
 */
	.section ".prefix.lib"
	.code16
	.globl	print_character
print_character:
	/* Preserve registers */
	pushw	%bx
	pushw	%bp
	/* Print character */
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	cmpb	$0x0a, %al		/* '\n'? */
	jne	1f
	int	$0x10
	movb	$0x0d, %al
1:	int	$0x10
	/* Restore registers and return */
	popw	%bp
	popw	%bx
	ret
	.size	print_character, . - print_character

/*****************************************************************************
 * Utility function: print a NUL-terminated string
 *
 * Parameters:
 *   %ds:si : string to print
 * Returns:
 *   %ds:si : character after terminating NUL
 *****************************************************************************
 */
	.section ".prefix.lib"
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
 * Returns:
 *   Nothing
 *****************************************************************************
 */
	.section ".prefix.lib"
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

/****************************************************************************
 * pm_call (real-mode near call)
 *
 * Call routine in 16-bit protected mode for access to extended memory
 *
 * Parameters:
 *   %ax : address of routine to call in 16-bit protected mode
 * Returns:
 *   none
 * Corrupts:
 *   %ax
 *
 * The specified routine is called in 16-bit protected mode, with:
 *
 *   %cs : 16-bit code segment with base matching real-mode %cs
 *   %ss : 16-bit data segment with base matching real-mode %ss
 *   %ds,%es,%fs,%gs : 32-bit data segment with zero base and 4GB limit
 *
 ****************************************************************************
 */

#ifndef KEEP_IT_REAL

	/* GDT for protected-mode calls */
	.section ".data16"
	.align 16
gdt:
gdt_limit:		.word gdt_length - 1
gdt_base:		.long 0
			.word 0 /* padding */
pm_cs:		/* 16-bit protected-mode code segment */
	.equ    PM_CS, pm_cs - gdt
	.word   0xffff, 0
	.byte   0, 0x9b, 0x00, 0
pm_ss:		/* 16-bit protected-mode stack segment */
	.equ    PM_SS, pm_ss - gdt
	.word   0xffff, 0
	.byte   0, 0x93, 0x00, 0
pm_ds:		/* 32-bit protected-mode flat data segment */
	.equ    PM_DS, pm_ds - gdt
	.word   0xffff, 0
	.byte   0, 0x93, 0xcf, 0
gdt_end:
	.equ	gdt_length, . - gdt
	.size	gdt, . - gdt

	.section ".data16"
	.align 16
pm_saved_gdt:	
	.long	0, 0
	.size	pm_saved_gdt, . - pm_saved_gdt

	.section ".prefix.lib"
	.code16
pm_call:
	/* Preserve registers, flags, GDT, and RM return point */
	pushfl
	sgdt	pm_saved_gdt
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs
	pushw	$99f

	/* Set up GDT bases */
	pushl	%eax
	pushw	%bx
	xorl	%eax, %eax
	movw	%ds, %ax
	shll	$4, %eax
	addl	$gdt, %eax
	movl	%eax, gdt_base
	movw	%cs, %ax
	movw	$pm_cs, %bx
	call	set_seg_base
	movw	%ss, %ax
	movw	$pm_ss, %bx
	call	set_seg_base
	popw	%bx
	popl	%eax

	/* Switch CPU to protected mode and load up segment registers */
	pushl	%eax
	cli
	lgdt	gdt
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	ljmp	$PM_CS, $1f
1:	movw	$PM_SS, %ax
	movw	%ax, %ss
	movw	$PM_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	popl	%eax

	/* Call PM routine */
	call	*%ax

	/* Set real-mode segment limits on %ds, %es, %fs and %gs */
	movw	%ss, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Return CPU to real mode */
	movl	%cr0, %eax
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0

	/* Restore registers and flags */
	lret	/* will ljmp to 99f */
99:	popw	%ss
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	lgdt	pm_saved_gdt
	popfl

	ret
	.size pm_call, . - pm_call

set_seg_base:
	rolw	$4, %ax
	movw	%ax, 2(%bx)
	andw	$0xfff0, 2(%bx)
	movb	%al, 4(%bx)
	andb	$0x0f, 4(%bx)
	ret
	.size set_seg_base, . - set_seg_base

#endif /* KEEP_IT_REAL */

/****************************************************************************
 * copy_bytes (real-mode or 16-bit protected-mode near call)
 *
 * Copy bytes
 *
 * Parameters:
 *   %ds:esi : source address
 *   %es:edi : destination address
 *   %ecx : length
 * Returns:
 *   %ds:esi : next source address
 *   %ds:esi : next destination address
 * Corrupts:
 *   None
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
copy_bytes:
	pushl %ecx
	rep addr32 movsb
	popl %ecx
	ret
	.size copy_bytes, . - copy_bytes

/****************************************************************************
 * install_block (real-mode or 16-bit protected-mode near call)
 *
 * Install block to specified address
 *
 * Parameters:
 *   %ds:esi : source address (must be a multiple of 16)
 *   %es:edi : destination address
 *   %ecx : length of (decompressed) data
 *   %edx : total length of block (including any uninitialised data portion)
 * Returns:
 *   %ds:esi : next source address (will be a multiple of 16)
 * Corrupts:
 *   %ecx, %edx
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
install_block:
	/* Preserve registers */
	pushl	%edi
	
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

	/* Round up %esi to start of next source block */
	addl	$0xf, %esi
	andl	$~0xf, %esi

	/* Restore registers and return */
	popl	%edi
	ret
	.size install_block, . - install_block
	
/****************************************************************************
 * alloc_basemem (real-mode near call)
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
	.section ".prefix.lib"
	.code16
	.globl	alloc_basemem
alloc_basemem:
	/* FBMS => %ax as segment address */
	movw	$0x40, %ax
	movw	%ax, %fs
	movw	%fs:0x13, %ax
	shlw	$6, %ax

	/* .data16 segment address */
	subw	$_data16_size_pgh, %ax
	pushw	%ax

	/* .text16 segment address */
	subw	$_text16_size_pgh, %ax
	pushw	%ax

	/* Update FBMS */
	shrw	$6, %ax
	movw	%ax, %fs:0x13

	/* Return */
	popw	%ax
	popw	%bx
	ret
	.size alloc_basemem, . - alloc_basemem

/****************************************************************************
 * install_basemem (real-mode near call)
 *
 * Install source block into base memory
 *
 * Parameters:
 *   %esi : source physical address (must be a multiple of 16)
 *   %es : destination segment address
 *   %cx : length of (decompressed) data
 *   %dx : total length of block (including any uninitialised data portion)
 * Returns:
 *   %esi : next source physical address (will be a multiple of 16)
 * Corrupts:
 *   %ecx, %edx
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
install_basemem:
	/* Preserve registers */
	pushl	%edi
	pushw	%ds

	/* Preserve original %esi */
	pushl	%esi

	/* Install to specified address */
	shrl	$4, %esi
	movw	%si, %ds
	xorw	%si, %si
	xorl	%edi, %edi
	movzwl	%cx, %ecx
	movzwl	%dx, %edx
	call	install_block

	/* Fix up %esi for return */
	popl	%ecx
	addl	%ecx, %esi

	/* Restore registers */
	popw	%ds
	popl	%edi
	ret
	.size install_basemem, . - install_basemem

/****************************************************************************
 * install_highmem (real-mode near call)
 *
 * Install source block into high memory
 *
 * Parameters:
 *   %esi : source physical address (must be a multiple of 16)
 *   %edi : destination physical address
 *   %ecx : length of (decompressed) data
 *   %edx : total length of block (including any uninitialised data portion)
 * Returns:
 *   %esi : next source physical address (will be a multiple of 16)
 * Corrupts:
 *   %ecx, %edx
 ****************************************************************************
 */

#ifndef KEEP_IT_REAL

	.section ".prefix.lib"
	.code16
install_highmem:
	/* Preserve registers */
	pushw	%ax

	/* Install to specified address */
	movw	$install_block, %ax
	call	pm_call

	/* Restore registers */
	popw	%ax
	ret
	.size install_highmem, . - install_highmem
	
#endif /* KEEP_IT_REAL */
	
/****************************************************************************
 * install (real-mode near call)
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
	.section ".prefix.lib"
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
 * install_prealloc (real-mode near call)
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
	.section ".prefix.lib"
	.code16
	.globl install_prealloc
install_prealloc:
	/* Save registers */
	pushal
	pushw	%ds
	pushw	%es

	/* Sanity: clear the direction flag asap */
	cld

	/* Calculate physical address of payload (i.e. first source) */
	testl	%esi, %esi
	jnz	1f
	movw	%cs, %si
	shll	$4, %esi
1:	addl	$_payload_offset, %esi

	/* Install .text16 */
	movw	%ax, %es
	movw	$_text16_size, %cx
	movw	%cx, %dx
	call	install_basemem

	/* Install .data16 */
	movw	%bx, %es
	movw	$_data16_progbits_size, %cx
	movw	$_data16_size, %dx
	call	install_basemem

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
	movl	$_textdata_progbits_size, %ecx
	movl	$_textdata_size, %edx
	pushl	%edi
	call	install_highmem
	popl	%edi

	/* Initialise librm at current location */
	movw	%ax, (init_librm_vector+2)
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
	pushw	%ax
	movw	$copy_bytes, %ax
	call	pm_call
	popw	%ax
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

	/* Vectors for far calls to .text16 functions */
	.section ".data16"
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


	/* File split information for the compressor */
#if COMPRESS
	.section ".zinfo", "a"
	.ascii	"COPY"
	.long	_prefix_load_offset
	.long	_prefix_progbits_size
	.long	_max_align
	.ascii	"PACK"
	.long	_text16_load_offset
	.long	_text16_progbits_size
	.long	_max_align
	.ascii	"PACK"
	.long	_data16_load_offset
	.long	_data16_progbits_size
	.long	_max_align
	.ascii	"PACK"
	.long	_textdata_load_offset
	.long	_textdata_progbits_size
	.long	_max_align
#else /* COMPRESS */
	.section ".zinfo", "a"
	.ascii	"COPY"
	.long	_prefix_load_offset
	.long	_load_size
	.long	_max_align
#endif /* COMPRESS */
