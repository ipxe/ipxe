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
 * Performance Monitor Cycle Counter (PMCCNTR)
 *
 */

	.section ".note.GNU-stack", "", %progbits
	.text
	.arm

/*
 * PMCCNTR status
 *
 * bit 31 	set if PMCCNTR availability is not yet determined
 * bit 0	set if PMCCNTR is available
 *
 */
	.section ".data.pmccntr_status", "aw", %progbits
	.globl	pmccntr_status
pmccntr_status:
	.word	0x80000000

/*
 * Check PMCCNTR availability
 *
 * Must preserve all registers, and return with either PMCCNTR enabled
 * or the Z flag set to indicate unavailability.
 *
 */
	.section ".text.pmccntr_check", "ax", %progbits
	.globl	pmccntr_check
	.type	pmccntr_check, %function
pmccntr_check:
	/* Save registers */
	stmfd	sp!, { r0, r1 }
	/* Read CPSR.M (bits 3:0, always permitted in PL0) */
	mrs	r0, cpsr
	and	r0, r0, #0x0000000f
	/* Read PMUSERENR.EN (bit 0, always permitted in PL0) */
	mrc	p15, 0, r1, c9, c14, 0
	and	r1, r1, #0x00000001
	/* Check if we are in PL1+ or in PL0 with PMUSERENR.EN set */
	orrs	r0, r0, r1
	/* If PMCCNTR is unavailable, exit with status=0 and ZF set */
	beq	1f
	/* Set PMCR.E (bit 0), set exit status=1 and ZF clear */
	movs	r0, #0x00000001
	mcr	p15, 0, r0, c9, c12, 0
	/* Set PMCNTENSET.C (bit 31) */
	mov	r1, #0x80000000
	mcr	p15, 0, r1, c9, c12, 1
1:	/* Store PMCCNTR status */
	ldr	r1, pmccntr_status_ptr
	str	r0, [r1]
	/* Restore registers and return */
	ldmfd	sp!, { r0, r1 }
	bx	lr
pmccntr_status_ptr:
	.word pmccntr_status
	.size	pmccntr_check, . - pmccntr_check
