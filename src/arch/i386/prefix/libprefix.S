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
	leal	(%edi,%ecx), %eax
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
	subw	$_data16_size, %ax
	pushw	%ax

	/* .text16 segment address */
	subw	$_text16_size, %ax
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

real_ds:	/* Genuine real mode data segment */
	.equ	REAL_DS, real_ds - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0, 0

flat_ds:	/* Flat real mode data segment */
	.equ	FLAT_DS, flat_ds - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

gdt_end:
	.equ	gdt_length, gdt_end - gdt
	.size gdt, . - gdt
	
#endif /* KEEP_IT_REAL */

/****************************************************************************
 * set_segment_limits (real-mode near call)
 *
 * Sets limits on the data segments %ds and %es.
 *
 * Parameters: 
 *   %cx : Segment limit ($REAL_DS or $FLAT_DS)
 ****************************************************************************
 */

#ifndef KEEP_IT_REAL
	
	.section ".prefix.lib"
	.code16
set_segment_limits:
	/* Preserve real-mode segment values and temporary registers */
	pushw	%es
	pushw	%ds
	pushl	%eax

	/* Set GDT base and load GDT */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	addl	$gdt, %eax
	movl	%eax, %cs:gdt_base
	lgdt	%cs:gdt

	/* Switch to protected mode, set segment limits, switch back */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	movw	%cx, %ds
	movw	%cx, %es
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0

	/* Restore real-mode segment values and temporary registers */
	popl	%eax
	popw	%ds
	popw	%es
	ret
	.size set_segment_limits, . - set_segment_limits
	
#endif /* KEEP_IT_REAL */
	
/****************************************************************************
 * install_highmem (real-mode near call)
 *
 * Install .text and .data into high memory
 *
 * Parameters:
 *   %edi : physical address in high memory
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
	/* Preserve registers and interrupt status */
	pushfl
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	pushl	%edx
		
	/* Disable interrupts and flatten real mode */
	cli
	movw	$FLAT_DS, %cx
	call	set_segment_limits

	/* Install .text and .data to specified address */
	xorw	%cx, %cx
	movw	%cx, %es
	movl	$_textdata_load_offset, %esi
	movl	$_textdata_progbits_size, %ecx
	movl	$_textdata_size, %edx
	call	install_block

	/* Unflatten real mode */
	movw	$REAL_DS, %cx
	call	set_segment_limits

	/* Restore registers and interrupt status */
	popl	%edx
	popl	%ecx
	popl	%edi
	popl	%esi
	popfl
	ret
	.size install_highmem, . - install_highmem
	
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

#ifndef KEEP_IT_REAL
	/* Install .text and .data to 2MB mark.  Use 2MB to avoid
	 * problems with A20.
	 */
	movl	$(2<<20), %edi
	call	install_highmem

	/* Continue executing in .text16 segment */
	movw	%bx, %ds
	pushw	%cs
	pushw	$2f
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "awx", @progbits
1:
	/* Set up protected-mode GDT, call relocate(), reset GDT */
	call	init_librm
	pushl	$relocate
	data32 call	prot_call
	addw	$4, %sp
	call	init_librm

	/* Return to executing in .prefix section */
	lret
	.section ".prefix.lib"
2:
#endif
	ret
	.size install_prealloc, . - install_prealloc
