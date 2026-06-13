/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Long jumps
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/*
 * Save stack context for non-local goto
 */
	.section ".text.setjmp", "ax", @progbits
	.globl	setjmp
setjmp:
	/* Save registers */
	stmg	%r6, %r15, 0(%r2)
	/* Return zero when returning as setjmp() */
	lghi	%r2, 0
	br	%r14
	.size	setjmp, . - setjmp

/*
 * Non-local jump to a saved stack context
 */
	.section ".text.longjmp", "ax", @progbits
	.globl	longjmp
longjmp:
	/* Restore registers */
	lmg	%r6, %r15, 0(%r2)
	/* Force result to non-zero */
	ltgr	%r2, %r3
	lochiz	%r2, 1
	/* Return to setjmp() caller */
	br	%r14
	.size	longjmp, . - longjmp
