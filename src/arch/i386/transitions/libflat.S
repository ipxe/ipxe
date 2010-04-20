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
 * flatten_real_mode
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
	ret
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

/****************************************************************************
 * test_a20_short, test_a20_long
 *
 * Check to see if A20 line is enabled
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if A20 line is not enabled
 * Corrupts:
 *   none
 ****************************************************************************
 */
#define TEST_A20_SHORT_MAX_RETRIES 0x20
#define TEST_A20_LONG_MAX_RETRIES 0x200000
	.section ".text16.early", "awx", @progbits
	.code16
test_a20_short:
	pushl	%ecx
	movl	$TEST_A20_SHORT_MAX_RETRIES, %ecx
	jmp	1f
	.size	test_a20_short, . - test_a20_short
test_a20_long:
	pushl	%ecx
	movl	$TEST_A20_LONG_MAX_RETRIES, %ecx
1:	pushw	%ax

	/* Flatten real mode so we can access the test pattern's 1MB offset */
	call	flatten_real_mode

2:	/* Modify and check test pattern; succeed if we see a difference */
	incw	%cs:test_a20_data
	addr32 movw %cs:(test_a20_data + 0x100000 ), %ax
	cmpw	%cs:test_a20_data, %ax
	clc
	jnz	99f

	/* Delay and retry */
	outb	%al, $0x80
	addr32 loop 2b
	stc

99:	/* Restore registers and return */
	popw	%ax
	popl	%ecx
	ret
	.size	test_a20_long, . - test_a20_long

	.section ".text16.early.data", "aw", @progbits
	.align	2
test_a20_data:
	.word	0xdead
	.size	test_a20_data, . - test_a20_data

/****************************************************************************
 * enable_a20_bios
 *
 * Try enabling A20 line via BIOS
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if A20 line is not enabled
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".text16.early", "awx", @progbits
	.code16
enable_a20_bios:
	/* Preserve registers */
	pushw	%ax

	/* Attempt INT 15,2401 */
	movw	$0x2401, %ax
	int	$0x15
	jc	99f

	/* Check that success was really successful */
	call	test_a20_short

99:	/* Restore registers and return */
	popw	%ax
	ret
	.size	enable_a20_bios, . - enable_a20_bios

/****************************************************************************
 * enable_a20_kbc
 *
 * Try enabling A20 line via keyboard controller
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if A20 line is not enabled
 * Corrupts:
 *   none
 ****************************************************************************
 */
#define KC_RDWR		0x60
#define KC_RDWR_SET_A20		0xdf
#define	KC_CMD		0x64
#define KC_CMD_WOUT		0xd1
#define KC_CMD_NULL		0xff
#define KC_STATUS	0x64
#define KC_STATUS_OBUF_FULL	0x01
#define KC_STATUS_IBUF_FULL	0x02
#define KC_MAX_RETRIES	100000
	.section ".text16.early", "awx", @progbits
	.code16
enable_a20_kbc:
	/* Preserve registers */
	pushw	%ax

	/* Try keyboard controller */
	call	empty_kbc
	movb	$KC_CMD_WOUT, %al
	outb	%al, $KC_CMD
	call	empty_kbc
	movb	$KC_RDWR_SET_A20, %al
	outb	%al, $KC_RDWR
	call	empty_kbc
	movb	$KC_CMD_NULL, %al
	outb	%al, $KC_CMD
	call	empty_kbc

	/* Check to see if it worked */
	call	test_a20_long

	/* Restore registers and return */
	popw	%ax
	ret
	.size	enable_a20_kbc, . - enable_a20_kbc

	.section ".text16.early", "awx", @progbits
	.code16
empty_kbc:
	/* Preserve registers */
	pushl	%ecx
	pushw	%ax

	/* Wait for KBC to become empty */
	movl	$KC_MAX_RETRIES, %ecx
1:	outb	%al, $0x80
	inb	$KC_STATUS, %al
	testb	$( KC_STATUS_OBUF_FULL | KC_STATUS_IBUF_FULL ), %al
	jz	99f
	testb	$KC_STATUS_OBUF_FULL, %al
	jz	2f
	outb	%al, $0x80
	inb	$KC_RDWR, %al
2:	addr32 loop 1b

99:	/* Restore registers and return */
	popw	%ax
	popl	%ecx
	ret
	.size	empty_kbc, . - empty_kbc

/****************************************************************************
 * enable_a20_fast
 *
 * Try enabling A20 line via "Fast Gate A20"
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if A20 line is not enabled
 * Corrupts:
 *   none
 ****************************************************************************
 */
#define SCP_A 0x92
	.section ".text16.early", "awx", @progbits
	.code16
enable_a20_fast:
	/* Preserve registers */
	pushw	%ax

	/* Try "Fast Gate A20" */
	inb	$SCP_A, %al
	orb	$0x02, %al
	andb	$~0x01, %al
	outb	%al, $SCP_A

	/* Check to see if it worked */
	call	test_a20_long

	/* Restore registers and return */
	popw	%ax
	ret
	.size	enable_a20_fast, . - enable_a20_fast

/****************************************************************************
 * enable_a20
 *
 * Try enabling A20 line via any available method
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if A20 line is not enabled
 * Corrupts:
 *   none
 ****************************************************************************
 */
#define ENABLE_A20_RETRIES 255
	.section ".text16.early", "awx", @progbits
	.code16
enable_a20:
	/* Preserve registers */
	pushl	%ecx
	pushw	%ax

	/* Check to see if A20 is already enabled */
	call	test_a20_short
	jnc	99f

	/* Use known working method, if we have one */
	movw	%cs:enable_a20_method, %ax
	testw	%ax, %ax
	jz	1f
	call	*%ax
	jmp	99f
1:
	/* Try all methods in turn until one works */
	movl	$ENABLE_A20_RETRIES, %ecx
2:	movw	$enable_a20_bios, %ax
	movw	%ax, %cs:enable_a20_method
	call	*%ax
	jnc	99f
	movw	$enable_a20_kbc, %ax
	movw	%ax, %cs:enable_a20_method
	call	*%ax
	jnc	99f
	movw	$enable_a20_fast, %ax
	movw	%ax, %cs:enable_a20_method
	call	*%ax
	jnc	99f
	addr32 loop 2b
	/* Failure; exit with carry set */
	movw	$0, %cs:enable_a20_method
	stc

99:	/* Restore registers and return */
	popw	%ax
	popl	%ecx
	ret

	.section ".text16.early.data", "aw", @progbits
	.align	2
enable_a20_method:
	.word	0
	.size	enable_a20_method, . - enable_a20_method

/****************************************************************************
 * access_highmem (real mode far call)
 *
 * Open up access to high memory in flat real mode with A20 enabled
 *
 * Parameters:
 *   none
 * Returns:
 *   CF set if high memory could not be accessed
 * Corrupts:
 *   none
 ****************************************************************************
 */
	.section ".text16.early", "awx", @progbits
	.code16
	.globl	access_highmem
access_highmem:
	/* Enable A20 line */
	call	enable_a20
	/* CPU will be in flat real mode as a result of this call */
	lret
	.size	access_highmem, . - access_highmem
