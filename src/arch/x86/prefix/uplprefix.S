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
 * Universal Payload (UPL) prefix
 *
 * The bootloader invokes a 32-bit universal payload in flat protected
 * mode with a device tree (or HOB list) pointer at ESP+4:
 *
 *   VOID PayloadEntry ( VOID *BootloaderParameter );
 *
 * The payload is a FIT image that the bootloader executes in place,
 * so we first copy ourselves to our link address.
 *
 */

	.section ".note.GNU-stack"
	.code32

/* Size of internal stack */
#define STACK_SIZE 0x10000

	.section ".prefix", "ax", @progbits
	.globl	_fit_start
_fit_start:
	/* We use no interrupts, and assume the direction flag is clear */
	cli
	cld

	/* Retrieve bootloader parameter (first cdecl parameter) */
	movl	4(%esp), %ebx

	/* Copy image to link address and continue execution there */
	call	1f
1:	popl	%esi
	subl	$1b, %esi
	addl	$_prefix, %esi
	movl	$_prefix, %edi
	movl	$_edata, %ecx
	subl	%edi, %ecx
	rep movsb
	movl	$2f, %eax
	jmp	*%eax
2:
	/* Zero .bss (including our stack) */
	movl	$_bss, %edi
	movl	$_ebss, %ecx
	subl	%edi, %ecx
	xorl	%eax, %eax
	rep stosb

	/* Switch to internal stack, since there is no way to return */
	movl	$upl_estack, %esp

	/* Install iPXE */
	pushl	%ebx
	call	upl_install
	addl	$4, %esp

	/* Call main program */
	call	main

	/* There is no return path to the bootloader: reboot */
	xorl	%eax, %eax
	call	reboot
3:	hlt
	jmp	3b
	.size	_fit_start, . - _fit_start

	/* End of identity-mapped physical address space */
	.section ".data.upl_mapped", "aw", @progbits
	.balign	8
	.globl	upl_mapped
upl_mapped:
	.quad	0x100000000
	.size	upl_mapped, . - upl_mapped

	/* Internal stack */
	.section ".stack", "aw", @nobits
	.balign	16
upl_stack:
	.space	STACK_SIZE
	.size	upl_stack, . - upl_stack
upl_estack:
