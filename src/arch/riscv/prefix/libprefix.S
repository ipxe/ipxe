/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * RISC-V prefix library
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/*****************************************************************************
 *
 * Apply relocation records
 *
 *****************************************************************************
 *
 * Apply relocation records from .rel.dyn to fix up iPXE to run at its
 * current address.
 *
 * This function must run before .bss is zeroed (since the relocation
 * records are overlaid with .bss).  It does not require a valid stack
 * pointer.
 *
 * Parameters: none (address is implicit in the program counter)
 *
 * Returns: none
 *
 */

/* Relative relocation type */
#define R_RISCV_RELATIVE 3

	/* Layout of a relocation record */
	.struct 0
rela_offset:	.space ( __riscv_xlen / 8 )
rela_type:	.space ( __riscv_xlen / 8 )
rela_addend:	.space ( __riscv_xlen / 8 )
rela_len:
	.previous

	.section ".prefix.apply_relocs", "ax", @progbits
	.globl	apply_relocs
apply_relocs:

	/* Get relocation records */
	la	t0, _reloc
	la	t1, _ereloc

	/* Determine current location */
	la	t2, reloc_base
	LOADN	t2, (t2)
	sub	t2, t0, t2

1:	/* Read relocation record */
	LOADN	t3, rela_offset(t0)
	LOADN	t4, rela_type(t0)
	LOADN	t5, rela_addend(t0)

	/* Check relocation type */
	addi	t4, t4, -R_RISCV_RELATIVE
	bnez	t4, 2f

	/* Apply relocation */
	add	t3, t3, t2
	add	t5, t5, t2
	STOREN	t5, (t3)
2:
	/* Loop until done */
	addi	t0, t0, rela_len
	blt	t0, t1, 1b

	/* Return to caller */
	ret
	.size	apply_relocs, . - apply_relocs

	/* Link-time address of _reloc */
	.section ".rodata", "a", @progbits
reloc_base:
	.dword	_reloc_base
	.size	reloc_base, . - reloc_base
