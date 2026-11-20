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
 * mode with a device tree (or HOB list) pointer at ESP+4, or a 64-bit
 * universal payload in long mode with the pointer in RCX:
 *
 *   VOID PayloadEntry ( VOID *BootloaderParameter );
 *
 * The payload is a FIT image that the bootloader executes in place,
 * so we first copy ourselves to our link address.
 *
 */

	.section ".note.GNU-stack"

/* Size of internal stack */
#define STACK_SIZE 0x10000

/* Size of identity map constructed by a 64-bit payload (in GB) */
#define IDENTITY_GB 64

/* Page table entry flags */
#define PG_P	0x01
#define PG_RW	0x02
#define PG_PS	0x80

	.section ".prefix", "ax", @progbits
	.globl	_fit_start
_fit_start:

#ifdef __x86_64__

	.code64

	/* We use no interrupts, and assume the direction flag is clear */
	cli
	cld

	/* Retrieve bootloader parameter (first MS ABI parameter) */
	movq	%rcx, %rbx

	/* Copy image to link address and continue execution there */
	leaq	_prefix(%rip), %rsi
	movq	$_prefix, %rdi
	movq	$_edata, %rcx
	subq	%rdi, %rcx
	rep movsb
	movq	$1f, %rax
	jmp	*%rax
1:
	/* Zero .bss (including our stack and page tables) */
	movq	$_bss, %rdi
	movq	$_ebss, %rcx
	subq	%rdi, %rcx
	xorl	%eax, %eax
	rep stosb

	/* Construct our own identity map using 2MB pages, since a
	 * 32-bit bootloader will have mapped only the first 4GB.
	 */
	movq	$upl_pd, %rdi
	movq	$( PG_P | PG_RW | PG_PS ), %rax
	movl	$( IDENTITY_GB * 512 ), %ecx
1:	stosq
	addq	$0x200000, %rax
	loop	1b
	movq	$upl_pdpt, %rdi
	movq	$( upl_pd + PG_P + PG_RW ), %rax
	movl	$IDENTITY_GB, %ecx
2:	stosq
	addq	$0x1000, %rax
	loop	2b
	movq	$( upl_pdpt + PG_P + PG_RW ), %rax
	movq	%rax, upl_pml4
	movq	$upl_pml4, %rax
	movq	%rax, %cr3

	/* Switch to internal stack, since there is no way to return */
	movq	$upl_estack, %rsp

	/* Install iPXE */
	movq	%rbx, %rdi
	call	upl_install

	/* Call main program */
	call	main

	/* There is no return path to the bootloader: reboot */
	xorl	%edi, %edi
	call	reboot

#else /* __x86_64__ */

	.code32

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

#endif /* __x86_64__ */

3:	hlt
	jmp	3b
	.size	_fit_start, . - _fit_start

	/* End of identity-mapped physical address space */
	.section ".data.upl_mapped", "aw", @progbits
	.balign	8
	.globl	upl_mapped
upl_mapped:
#ifdef __x86_64__
	.quad	( IDENTITY_GB << 30 )
#else
	.quad	0x100000000
#endif
	.size	upl_mapped, . - upl_mapped

#ifdef __x86_64__
	/* Identity-mapped page tables */
	.section ".bss.upl_pages", "aw", @nobits
	.balign	4096
upl_pml4:
	.space	4096
upl_pdpt:
	.space	4096
upl_pd:
	.space	( IDENTITY_GB * 4096 )
#endif

	/* Internal stack */
	.section ".stack", "aw", @nobits
	.balign	16
upl_stack:
	.space	STACK_SIZE
	.size	upl_stack, . - upl_stack
upl_estack:
