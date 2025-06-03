/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Multiprocessor functions
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/* Selectively assemble code for 32-bit/64-bit builds */
#if defined ( __x86_64__ ) && ! defined ( PLATFORM_pcbios )
#define codemp code64
#define DI rdi
#define SP rsp
#define if32 if 0
#define if64 if 1
#else
#define codemp code32
#define DI edi
#define SP esp
#define if32 if 1
#define if64 if 0
#endif

/* Standard features CPUID leaf */
#define CPUID_FEATURES 0x00000001

/* x2APIC is supported */
#define CPUID_FEATURES_ECX_X2APIC 0x00200000

/* Extended topology enumeration CPUID leaf */
#define CPUID_XT_ENUM 0x0000000b

/*
 * Call multiprocessor function from C code
 *
 * Parameters:
 *	4(%esp)/%rdi	Multiprocessor function
 *	8(%esp)/%rsi	Opaque data pointer
 */
	.section ".text.mp_call", "ax", @progbits
	.codemp
	.globl	mp_call
mp_call:
.if64	/* Preserve registers, load incoming parameters into registers */
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rbx
	pushq	%rsp
	pushq	%rbp
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
.else
	pushal
	movl	36(%esp), %eax
	movl	40(%esp), %edx
.endif
	/* Call multiprocessor function */
	call	mp_jump

.if64	/* Restore registers and return */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rbp
	leaq	8(%rsp), %rsp /* discard */
	popq	%rbx
	popq	%rdx
	popq	%rcx
	popq	%rax
.else
	popal
.endif
	ret
	.size	mp_call, . - mp_call

/*
 * Jump to multiprocessor function
 *
 * Parameters:
 *	%eax/%rdi	Multiprocessor function
 *	%edx/%rsi	Opaque data pointer
 *	%esp/%rsp	Stack, or NULL to halt AP upon completion
 *
 * Obtain the CPU identifier (i.e. the APIC ID) and perform a tail
 * call into the specified multiprocessor function.
 *
 * This code may run with no stack on an application processor.
 */
	.section ".text.mp_jump", "ax", @progbits
	.codemp
	.globl	mp_jump
mp_jump:
.if32	/* Move function parameters to available registers */
	movl	%eax, %edi
	movl	%edx, %esi
.endif

	/* Get 8-bit APIC ID and x2APIC feature bit */
	movl	$CPUID_FEATURES, %eax
	cpuid
	shrl	$24, %ebx
	movl	%ebx, %edx

	/* Get 32-bit x2APIC ID if applicable */
	testl	$CPUID_FEATURES_ECX_X2APIC, %ecx
	jz	1f
	movl	$CPUID_XT_ENUM, %eax
	xorl	%ecx, %ecx
	cpuid
1:

.if64	/* Tail call to function */
	movq	%rdi, %rax
	movq	%rsi, %rdi
	movl	%edx, %esi
	jmp	*%rax
.else
	movl	%esi, %eax
	jmp	*%edi
.endif
	.size	mp_jump, . - mp_jump

/*
 * Update maximum CPU identifier
 *
 * Parameters:
 *	%eax/%rdi	Pointer to shared maximum APIC ID
 *	%edx/%rsi	CPU identifier (APIC ID)
 *	%esp/%rsp	Stack, or NULL to halt AP upon completion
 *
 * This code may run with no stack on an application processor.
 */
	.section ".text.mp_update_max_cpuid", "ax", @progbits
	.codemp
	.globl	mp_update_max_cpuid
mp_update_max_cpuid:
.if32	/* Move function parameters to available registers */
	movl	%eax, %edi
	movl	%edx, %esi
.endif
	/* Update maximum APIC ID (atomically) */
	movl	(%DI), %eax
1:	cmpl	%esi, %eax
	jae	2f
	lock cmpxchgl %esi, (%DI)
	jnz	1b
2:
	/* Return to caller (if stack exists), or halt application processor */
	test	%SP, %SP
	jz	3f
	ret
3:	cli
	hlt
	jmp	3b
	.size	mp_update_max_cpuid, . - mp_update_max_cpuid
