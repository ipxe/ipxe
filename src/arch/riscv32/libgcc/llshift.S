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
 * Long shifts
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/**
 * Shift left
 *
 * @v a1:a0		Value to shift
 * @v a2		Shift amount
 * @ret a1:a0		Shifted value
 */
	.section ".text.__ashldi3", "ax", @progbits
	.globl	__ashldi3
__ashldi3:
	/* Perform shift by 32 bits, if applicable */
	li	t0, 32
	sub	t1, t0, a2
	bgtz	t1, 1f
	mv	a1, a0
	mv	a0, zero
1:	/* Perform shift by modulo-32 bits, if applicable */
	andi	a2, a2, 0x1f
	beqz	a2, 2f
	srl	t2, a0, t1
	sll	a0, a0, a2
	sll	a1, a1, a2
	or	a1, a1, t2
2:	ret
	.size	__ashldi3, . - __ashldi3

/**
 * Logical shift right
 *
 * @v a1:a0		Value to shift
 * @v a2		Shift amount
 * @ret a1:a0		Shifted value
 */
	.section ".text.__lshrdi3", "ax", @progbits
	.globl	__lshrdi3
__lshrdi3:
	/* Perform shift by 32 bits, if applicable */
	li	t0, 32
	sub	t1, t0, a2
	bgtz	t1, 1f
	mv	a0, a1
	mv	a1, zero
1:	/* Perform shift by modulo-32 bits, if applicable */
	andi	a2, a2, 0x1f
	beqz	a2, 2f
	sll	t2, a1, t1
	srl	a1, a1, a2
	srl	a0, a0, a2
	or	a0, a0, t2
2:	ret
	.size	__lshrdi3, . - __lshrdi3

/**
 * Arithmetic shift right
 *
 * @v a1:a0		Value to shift
 * @v a2		Shift amount
 * @ret a1:a0		Shifted value
 */
	.section ".text.__ashrdi3", "ax", @progbits
	.globl	__ashrdi3
__ashrdi3:
	/* Perform shift by 32 bits, if applicable */
	li	t0, 32
	sub	t1, t0, a2
	bgtz	t1, 1f
	mv	a0, a1
	srai	a1, a1, 16
	srai	a1, a1, 16
1:	/* Perform shift by modulo-32 bits, if applicable */
	andi	a2, a2, 0x1f
	beqz	a2, 2f
	sll	t2, a1, t1
	sra	a1, a1, a2
	srl	a0, a0, a2
	or	a0, a0, t2
2:	ret
	.size	__ashrdi3, . - __ashrdi3
