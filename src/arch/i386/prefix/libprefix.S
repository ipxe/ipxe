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

#define CR0_PE 1

	.arch i386
	.section ".prefix.lib", "awx", @progbits
	.section ".data16", "aw", @progbits

/****************************************************************************
 * install_block (real-mode near call)
 *
 * Install block to specified address
 *
 * Parameters:
 *   %esi : byte offset within loaded image (must be a multiple of 16)
 *   %es:edi : destination address
 *   %ecx : length of (decompressed) data
 *   %edx : total length of block (including any uninitialised data portion)
 * Returns:
 *   none
 * Corrupts:
 *   %esi, %edi, %ecx, %edx
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
install_block:
	/* Preserve registers */
	pushw	%ds
	pushl	%eax
	
	/* Starting segment => %ds */
	movw	%cs, %ax
	shrl	$4, %esi
	addw	%si, %ax
	movw	%ax, %ds
	xorl	%esi, %esi

	/* Calculate start and length of uninitialised data portion */
	addr32 leal	(%edi,%ecx), %eax
	subl	%ecx, %edx
	
	/* Do the copy */
	cld
	rep addr32 movsb /* or "call decompress16" */

	/* Zero remaining space */
	movl	%eax, %edi
	movl	%edx, %ecx
	xorb	%al, %al
	rep addr32 stosb

	/* Restore registers */
	popl	%eax
	popw	%ds
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
 * Install .text16 and .data16 into base memory
 *
 * Parameters: 
 *   %ax : .text16 segment address
 *   %bx : .data16 segment address
 * Returns:
 *   none
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
install_basemem:
	/* Preserve registers */
	pushw	%es
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	pushl	%edx

	/* Install .text16 */
	movw	%ax, %es
	xorl	%edi, %edi
	movl	$_text16_load_offset, %esi
	movl	$_text16_size, %ecx
	movl	%ecx, %edx
	call	install_block

	/* Install .data16 */
	movw	%bx, %es
	xorl	%edi, %edi	
	movl	$_data16_load_offset, %esi
	movl	$_data16_progbits_size, %ecx
	movl	$_data16_size, %edx
	call	install_block

	/* Restore registers */
	popl	%edx
	popl	%ecx
	popl	%edi
	popl	%esi
	popw	%es
	ret
	.size install_basemem, . - install_basemem

/****************************************************************************
 * install_highmem (flat real-mode near call)
 *
 * Install .text and .data into high memory
 *
 * Parameters:
 *   %es:edi : address in high memory
 * Returns:
 *   none
 * Corrupts:
 *   none
 ****************************************************************************
 */

#ifndef KEEP_IT_REAL

	.section ".prefix.lib"
	.code16
install_highmem:
	/* Preserve registers */
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	pushl	%edx
		
	/* Install .text and .data to specified address */
	movl	$_textdata_load_offset, %esi
	movl	$_textdata_progbits_size, %ecx
	movl	$_textdata_size, %edx
	call	install_block

	/* Restore registers and interrupt status */
	popl	%edx
	popl	%ecx
	popl	%edi
	popl	%esi
	ret
	.size install_highmem, . - install_highmem
	
#endif /* KEEP_IT_REAL */
	
/****************************************************************************
 * GDT for flat real mode
 *
 * We only ever use this GDT to set segment limits; the bases are
 * unused.  Also, we only flatten data segments, so we don't need to
 * worry about the code or stack segments.  This makes everything much
 * simpler.
 ****************************************************************************
 */
	
#ifndef KEEP_IT_REAL
	
	.section ".prefix.lib"
	.align 16
gdt:
gdt_limit:		.word gdt_length - 1
gdt_base:		.long 0
			.word 0 /* padding */

flat_ds:	/* Flat real mode data segment */
	.equ	FLAT_DS, flat_ds - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

gdt_end:
	.equ	gdt_length, gdt_end - gdt
	.size gdt, . - gdt
	
#endif /* KEEP_IT_REAL */

/****************************************************************************
 * flatten_real_mode (real-mode near call)
 *
 * Sets 4GB limits on the data segments %ds and %es.
 *
 * Parameters: 
 *   none
 ****************************************************************************
 */

#ifndef KEEP_IT_REAL
	
	.section ".prefix.lib"
	.code16
flatten_real_mode:
	/* Preserve real-mode segment values and temporary registers */
	pushw	%es
	pushw	%ds
	pushw	%bp
	pushl	%eax

	/* Set GDT base and load GDT */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	addl	$gdt, %eax
	pushl	%eax
	pushw	%cs:gdt_limit
	movw	%sp, %bp
	lgdt	(%bp)
	addw	$6, %sp

	/* Switch to protected mode */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	/* Set flat segment limits */
	movw	$FLAT_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es

	/* Switch back to real mode */
	movl	%cr0, %eax
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0

	/* Restore real-mode segment values and temporary registers */
	popl	%eax
	popw	%bp
	popw	%ds
	popw	%es
	ret
	.size flatten_real_mode, . - flatten_real_mode
	
#endif /* KEEP_IT_REAL */
	
/****************************************************************************
 * install (real-mode near call)
 * install_prealloc (real-mode near call)
 *
 * Install all text and data segments.
 *
 * Parameters:
 *   %ax : .text16 segment address (install_prealloc only)
 *   %bx : .data16 segment address (install_prealloc only)
 * Returns:
 *   %ax : .text16 segment address
 *   %bx : .data16 segment address
 *   %edi : .text physical address (if applicable)
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
	.globl install
install:
	/* Allocate space for .text16 and .data16 */
	call	alloc_basemem
	.size install, . - install
	.globl install_prealloc
install_prealloc:
	/* Install .text16 and .data16 */
	call	install_basemem

#ifdef KEEP_IT_REAL
	/* Preserve %ds, call init_libkir, restore registers */
	pushw	%ds
	movw	%bx, %ds
	movw	%ax, (init_libkir_vector+2)
	lcall	*init_libkir_vector
	popw	%ds
#else
	/* Preserve registers and interrupt status, and disable interrupts */
	pushfw
	pushw	%ds
	pushw	%es
	pushl	%esi
	pushl	%ecx
	cli

	/* Load up %ds and %es, and set up vectors for far calls to .text16 */
	movw	%bx, %ds
	xorw	%si, %si
	movw	%si, %es
	movw	%ax, (init_librm_vector+2)
	movw	%ax, (prot_call_vector+2)
	
	/* Install .text and .data to 2MB mark.  Use 2MB to avoid
	 * problems with A20.
	 */
	call	flatten_real_mode
	movl	$(2<<20), %edi
	call	install_highmem

	/* Set up initial protected-mode GDT, call relocate().
	 * relocate() will return with %esi, %edi and %ecx set up
	 * ready for the copy to the new location.
	 */
	lcall	*init_librm_vector
	pushl	$relocate
	lcall	*prot_call_vector
	addw	$4, %sp

	/* Move code to new location, set up new protected-mode GDT */
	call	flatten_real_mode
	pushl	%edi
	es rep addr32 movsb
	popl	%edi
	lcall	*init_librm_vector
	
	/* Hide Etherboot from BIOS memory map.  Note that making this
	 * protected-mode call will also restore normal (non-flat)
	 * real mode, as part of the protected-to-real transition.
	 */
	pushl	$hide_etherboot
	lcall	*prot_call_vector
	addw	$4, %sp

	/* Restore registers and interrupt status */
	popl	%ecx
	popl	%esi
	popw	%es
	popw	%ds
	popfw
#endif
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
