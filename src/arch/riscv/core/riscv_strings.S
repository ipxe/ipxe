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

/**
 * Find first (i.e. least significant) set bit
 *
 * @v value		Value
 * @ret lsb		Least significant bit set in value (LSB=1), or zero
 */
	.section ".text.riscv_ffs"
	.globl	riscv_ffs
riscv_ffs:
	beqz	a0, 2f
	mv	t0, a0
	li	a0, ( __riscv_xlen + 1 )
1:	slli	t0, t0, 1
	addi	a0, a0, -1
	bnez	t0, 1b
2:	ret
	.size	riscv_ffs, . - riscv_ffs

/**
 * Find last (i.e. most significant) set bit
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
	.section ".text.riscv_fls"
	.globl	riscv_fls
riscv_fls:
	beqz	a0, 2f
	mv	t0, a0
	li	a0, __riscv_xlen
	bltz	t0, 2f
1:	slli	t0, t0, 1
	addi	a0, a0, -1
	bgez	t0, 1b
2:	ret
	.size	riscv_fls, . - riscv_fls
