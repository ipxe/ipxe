/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

	FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

/** @file
 *
 * Jumps to 32-bit operating system entry points
 *
 */

	.section ".note.GNU-stack"

/* Selectors required by the Linux 32-bit boot protocol */
#define UPL_CS32	0x10
#define UPL_DS		0x18

/* Load flat data segments */
.macro	load_flat_data
	movw	$UPL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
.endm

/*
 * Jump to 32-bit operating system entry point
 *
 * Declared as __asmcall void upl_jump_ia32 ( uint32_t entry,
 *					      uint32_t params )
 *
 * The parameter is passed in %esi, as required by the Linux 32-bit
 * boot protocol.
 */
	.section ".text.upl_jump_ia32", "ax", @progbits
	.code32
	.globl	upl_jump_ia32
upl_jump_ia32:
	cli
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	lgdt	upl_gdtr
	ljmp	$UPL_CS32, $1f
1:	load_flat_data
	movl	%edi, %eax
	xorl	%ebp, %ebp
	xorl	%edi, %edi
	xorl	%ebx, %ebx
	jmp	*%eax
	.size	upl_jump_ia32, . - upl_jump_ia32

	.section ".data.upl_gdt", "aw", @progbits
	.balign	16
upl_gdt:
	.quad	0x0000000000000000	/* Null */
	.quad	0x0000000000000000	/* Unused */
	.quad	0x00cf9a000000ffff	/* UPL_CS32: 4GB flat 32-bit code */
	.quad	0x00cf92000000ffff	/* UPL_DS: 4GB flat data */
upl_gdtr:
	.word	( upl_gdtr - upl_gdt - 1 )
	.long	upl_gdt
	.long	0
