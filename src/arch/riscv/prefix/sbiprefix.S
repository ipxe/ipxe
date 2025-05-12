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
 * SBI position-independent executable prefix
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/* Page size */
#define PAGE_SIZE 4096

	/*
	 * SBI entry point
	 */
	.section ".prefix", "ax", @progbits
	.org 0
	.globl	_sbi_start
_sbi_start:
	/* Identify temporary page table and stack space
	 *
	 * Assume that there is sufficient writable memory (~8kB)
	 * directly below the device tree.
	 */
	li	t0, ~( PAGE_SIZE - 1 )
	and	sp, a1, t0
	li	t0, PAGE_SIZE
	sub	sp, sp, t0
	mv	a2, sp

	/* Install iPXE */
	call	install

	/* Call main program */
	call	main

	/* We have no return path, since the M-mode SBI implementation
	 * will have jumped to us by setting our start address in MEPC
	 * and issuing an MRET instruction.
	 *
	 * Attempt a system reset, since there is nothing else we can
	 * viably do at this point.
	 */
	j	reset_system
	.size	_sbi_start, . - _sbi_start
