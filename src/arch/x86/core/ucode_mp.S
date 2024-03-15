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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Microcode updates
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/* Selectively assemble code for 32-bit/64-bit builds */
#if defined ( __x86_64__ ) && ! defined ( PLATFORM_pcbios )
#define codemp code64
#define AX rax
#define BX rbx
#define CX rcx
#define DX rdx
#define SI rsi
#define DI rdi
#define BP rbp
#define SP rsp
#define if32 if 0
#define if64 if 1
#else
#define codemp code32
#define AX eax
#define BX ebx
#define CX ecx
#define DX edx
#define SI esi
#define DI edi
#define BP ebp
#define SP esp
#define if32 if 1
#define if64 if 0
#endif

/* Standard features CPUID leaf */
#define CPUID_FEATURES 0x00000001

/* BIOS update signature MSR */
#define MSR_BIOS_SIGN_ID 0x0000008b

/** Microcode update control layout
 *
 * This must match the layout of struct ucode_control.
 */
	.struct	0
CONTROL_DESC:
	.space	8
CONTROL_STATUS:
	.space	8
CONTROL_TRIGGER_MSR:
	.space	4
CONTROL_APIC_MAX:
	.space	4
CONTROL_APIC_UNEXPECTED:
	.space	4
CONTROL_APIC_MASK:
	.space	4
CONTROL_APIC_TEST:
	.space	4
CONTROL_VER_CLEAR:
	.space	1
CONTROL_VER_HIGH:
	.space	1
CONTROL_LEN:

/* We use register %ebp/%rbp to hold the address of the update control */
#define CONTROL BP

/* Microcode update descriptor layout
 *
 * This must match the layout of struct ucode_descriptor.
 */
	.struct	0
DESC_SIGNATURE:
	.space	4
DESC_VERSION:
	.space	4
DESC_ADDRESS:
	.space	8
DESC_LEN:

/* We use register %esi/%rsi to hold the address of the descriptor */
#define DESC SI

/** Microcode update status report layout
 *
 * This must match the layout of struct ucode_status.
 */
	.struct	0
STATUS_SIGNATURE:
	.space	4
STATUS_ID:
	.space	4
STATUS_BEFORE:
	.space	4
STATUS_AFTER:
	.space	4
STATUS_LEN:
	.equ	LOG2_STATUS_LEN, 4
	.if	( 1 << LOG2_STATUS_LEN ) - STATUS_LEN
	.error	"LOG2_STATUS_LEN value is incorrect"
	.endif

/* We use register %edi/%rdi to hold the address of the status report */
#define STATUS DI

/*
 * Update microcode
 *
 * Parameters:
 *	%eax/%rdi	Microcode update structure
 *	%edx/%rsi	CPU identifier (APIC ID)
 *	%esp/%rsp	Stack, or NULL to halt AP upon completion
 *
 * This code may run with no stack on an application processor (AP).
 * All values must be held in registers, and no subroutine calls are
 * possible.  No firmware routines may be called.
 *
 * Since cpuid/rdmsr/wrmsr require the use of %eax, %ebx, %ecx, and
 * %edx, we have essentially only three registers available for
 * long-term state.
 */
	.text
	.globl	ucode_update
	.codemp
	.section ".text.ucode_update", "ax", @progbits
ucode_update:

.if64	/* Get input parameters */
	movq	%rdi, %CONTROL
	movl	%esi, %edx
.else
	movl	%eax, %CONTROL
.endif
	/* Check against maximum expected APIC ID */
	cmpl	CONTROL_APIC_MAX(%CONTROL), %edx
	jbe	1f
	movl	%edx, CONTROL_APIC_UNEXPECTED(%CONTROL)
	jmp	done
1:
	/* Calculate per-CPU status report buffer address */
	mov	%DX, %STATUS
	shl	$LOG2_STATUS_LEN, %STATUS
	add	CONTROL_STATUS(%CONTROL), %STATUS

	/* Report APIC ID */
	movl	%edx, STATUS_ID(%STATUS)

	/* Get and report CPU signature */
	movl	$CPUID_FEATURES, %eax
	cpuid
	movl	%eax, STATUS_SIGNATURE(%STATUS)

	/* Check APIC ID mask */
	movl	STATUS_ID(%STATUS), %eax
	andl	CONTROL_APIC_MASK(%CONTROL), %eax
	cmpl	CONTROL_APIC_TEST(%CONTROL), %eax
	jne	done

	/* Clear BIOS_SIGN_ID MSR if applicable */
	movl	$MSR_BIOS_SIGN_ID, %ecx
	xorl	%eax, %eax
	xorl	%edx, %edx
	testb	$0xff, CONTROL_VER_CLEAR(%CONTROL)
	jz	1f
	wrmsr
1:
	/* Get CPU signature to repopulate BIOS_SIGN_ID MSR (for Intel) */
	movl	$CPUID_FEATURES, %eax
	cpuid

	/* Get initial microcode version */
	movl	$MSR_BIOS_SIGN_ID, %ecx
	rdmsr
	testb	$0xff, CONTROL_VER_HIGH(%CONTROL)
	jz	1f
	movl	%edx, %eax
1:	movl	%eax, STATUS_BEFORE(%STATUS)

	/* Get start of descriptor list */
	mov	CONTROL_DESC(%CONTROL), %DESC
	sub	$DESC_LEN, %DESC

1:	/* Walk update descriptor list to find a matching CPU signature */
	add	$DESC_LEN, %DESC
	movl	DESC_SIGNATURE(%DESC), %eax
	testl	%eax, %eax
	jz	noload
	cmpl	STATUS_SIGNATURE(%STATUS), %eax
	jne	1b

	/* Compare (signed) microcode versions */
	movl	STATUS_BEFORE(%STATUS), %eax
	cmpl	DESC_VERSION(%DESC), %eax
	jge	noload

	/* Load microcode update */
	movl	CONTROL_TRIGGER_MSR(%CONTROL), %ecx
	movl	(DESC_ADDRESS + 0)(%DESC), %eax
	movl	(DESC_ADDRESS + 4)(%DESC), %edx
	wrmsr

noload:	/* Clear BIOS_SIGN_ID MSR if applicable */
	movl	$MSR_BIOS_SIGN_ID, %ecx
	xorl	%eax, %eax
	xorl	%edx, %edx
	testb	$0xff, CONTROL_VER_CLEAR(%CONTROL)
	jz	1f
	wrmsr
1:
	/* Get CPU signature to repopulate BIOS_SIGN_ID MSR (for Intel) */
	movl	$CPUID_FEATURES, %eax
	cpuid

	/* Get and report final microcode version */
	movl	$MSR_BIOS_SIGN_ID, %ecx
	rdmsr
	testb	$0xff, CONTROL_VER_HIGH(%CONTROL)
	jz	1f
	movl	%edx, %eax
1:	movl	%eax, STATUS_AFTER(%STATUS)

done:	/* Return to caller (if stack exists), or halt application processor */
	test	%SP, %SP
	jz	1f
	ret
1:	cli
	hlt
	jmp	1b
	.size	ucode_update, . - ucode_update
