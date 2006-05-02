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
 *   %ecx : length to install
 * Returns:
 *   none
 * Corrupts:
 *   %esi, %edi, %ecx
 ****************************************************************************
 */
	.section ".prefix.lib"
	.code16
install_block:
	/* Preserve registers */
	pushw	%ds
	pushw	%ax
	
	/* Starting segment => %ds */
	movw	%cs, %ax
	shrl	$4, %esi
	addw	%si, %ax
	movw	%ax, %ds
	xorl	%esi, %esi

	/* Do the copy */
	cld
	addr32 rep movsb /* or "call decompress16" */

	/* Restore registers */
	popw	%ax
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
	.globl alloc_basemem
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
	.globl install_basemem
install_basemem:
	/* Preserve registers */
	pushw	%es
	pushl	%esi
	pushl	%edi
	pushl	%ecx

	/* Install .text16 */
	movw	%ax, %es
	xorl	%edi, %edi
	movl	$_text16_load_offset, %esi
	movl	$_text16_size, %ecx
	call	install_block

	/* Install .data16 */
	movw	%bx, %es
	xorl	%edi, %edi	
	movl	$_data16_load_offset, %esi
	movl	$_data16_progbits_size, %ecx
	call	install_block

	/* Restore registers */
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

/****************************************************************************
 * set_segment_limits (real-mode near call)
 *
 * Sets limits on the data segments %ds and %es.
 *
 * Parameters: 
 *   %cx : Segment limit ($REAL_DS or $FLAT_DS)
 ****************************************************************************
 */
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
	.section ".prefix.lib"
	.code16
	.globl install_highmem
install_highmem:
	/* Preserve registers and interrupt status */
	pushfl
	pushl	%esi
	pushl	%edi
	pushl	%ecx
		
	/* Disable interrupts and flatten real mode */
	cli
	movw	$FLAT_DS, %cx
	call	set_segment_limits

	/* Install .text and .data to specified address */
	xorw	%cx, %cx
	movw	%cx, %es
	movl	$_text_load_offset, %esi
	movl	$_text_progbits_size, %ecx
	call	install_block

	/* Unflatten real mode */
	movw	$REAL_DS, %cx
	call	set_segment_limits

	/* Restore registers and interrupt status */
	popl	%ecx
	popl	%edi
	popl	%esi
	popfl
	ret
	.size install_highmem, . - install_highmem
