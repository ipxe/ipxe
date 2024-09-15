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
 * Byte swapping
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

	.section ".text.riscv_swap", "ax", @progbits
riscv_swap:
	.globl	riscv_swap_word
	.globl	riscv_swap_half
	.globl	riscv_swap_byte
riscv_swap_word:
	/* Swap low and high words of a0 */
	slli	t0, a0, 32
	srli	a0, a0, 32
	or	a0, a0, t0
riscv_swap_half:
	/* Swap half-words within each word of a0 */
	ld	t1, mask16
	slli	t2, a0, 16
	and	a0, a0, t1
	and	t2, t2, t1
	srli	a0, a0, 16
	or	a0, a0, t2
riscv_swap_byte:
	/* Swap bytes within each half-word of a0 */
	ld	t3, mask8
	slli	t4, a0, 8
	and	a0, a0, t3
	and	t4, t4, t3
	srli	a0, a0, 8
	or	a0, a0, t4
	ret
mask16:	.dword 0xffff0000ffff0000
mask8:	.dword 0xff00ff00ff00ff00
	.size	riscv_swap, . - riscv_swap
