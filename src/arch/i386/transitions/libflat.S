/*
 * Copyright (C) 2010 Michael Brown <mbrown@fensystems.co.uk>.
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

#define CR0_PE 1

/****************************************************************************
 * flatten_real_mode (real-mode far call)
 *
 * Set up 4GB segment limits
 *
 * Parameters:
 *   none
 * Returns:
 *   none
 * Corrupts:
 *   none
 ****************************************************************************
 */
	/* GDT for protected-mode calls */
	.section ".text16.early.data", "aw", @progbits
	.align 16
flatten_gdt:
flatten_gdt_limit:	.word flatten_gdt_length - 1
flatten_gdt_base:	.long 0
			.word 0 /* padding */
flatten_cs:	/* 16-bit protected-mode flat code segment */
	.equ    FLAT_CS, flatten_cs - flatten_gdt
	.word   0xffff, 0
	.byte   0, 0x9b, 0x8f, 0
flatten_ss:	/* 16-bit protected-mode flat stack segment */
	.equ    FLAT_SS, flatten_ss - flatten_gdt
	.word   0xffff, 0
	.byte   0, 0x93, 0x8f, 0
flatten_gdt_end:
	.equ	flatten_gdt_length, . - flatten_gdt
	.size	flatten_gdt, . - flatten_gdt

	.section ".text16.early.data", "aw", @progbits
	.align 16
flatten_saved_gdt:
	.long	0, 0
	.size	flatten_saved_gdt, . - flatten_saved_gdt

	.section ".text16.early", "awx", @progbits
	.code16
	.globl	flatten_real_mode
flatten_real_mode:
	/* Preserve registers and flags */
	pushfl
	pushl	%eax
	pushw	%si
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss

	/* Set %ds for access to .text16.early.data variables */
	pushw	%cs
	popw	%ds

	/* Preserve original GDT */
	sgdt flatten_saved_gdt

	/* Set up GDT bases */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	addl	$flatten_gdt, %eax
	movl	%eax, flatten_gdt_base
	movw	%cs, %ax
	movw	$flatten_cs, %si
	call	set_seg_base
	movw	%ss, %ax
	movw	$flatten_ss, %si
	call	set_seg_base

	/* Switch temporarily to protected mode and set segment registers */
	pushw	%cs
	pushw	$2f
	cli
	data32 lgdt flatten_gdt
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	ljmp	$FLAT_CS, $1f
1:	movw	$FLAT_SS, %ax
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movl	%cr0, %eax
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0
	lret
2:	/* lret will ljmp to here */

	/* Restore GDT, registers and flags */
	data32 lgdt flatten_saved_gdt
	popw	%ss
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	popw	%si
	popl	%eax
	popfl
	lret
	.size flatten_real_mode, . - flatten_real_mode

	.section ".text16.early", "awx", @progbits
	.code16
set_seg_base:
	rolw	$4, %ax
	movw	%ax, 2(%si)
	andw	$0xfff0, 2(%si)
	movb	%al, 4(%si)
	andb	$0x0f, 4(%si)
	ret
	.size set_seg_base, . - set_seg_base
