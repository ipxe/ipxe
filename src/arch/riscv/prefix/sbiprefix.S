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

/* ELF machine type */
#define EM_RISCV 243

	/*
	 * Display progress message via debug console
	 */
	.macro	progress message
#ifndef NDEBUG
	.section ".rodata.progress_\@", "a", @progbits
progress_\@:
	.asciz	"\message"
	.size	progress_\@, . - progress_\@
	.previous
	la	t1, progress_\@
	call	print_message
#endif
	.endm

	/*
	 * SBI entry point
	 */
	.section ".prefix", "ax", @progbits
	.org 0
	.globl	_sbi_start
_sbi_start:
	/* Preserve arguments */
	mv	s0, a0
	mv	s1, a1
	progress "\nSBI->iPXE"

	/* Apply dynamic relocations */
	call	apply_relocs
	progress " .reloc"

	/* Zero the bss */
	la	t0, _bss
	la	t1, _ebss
1:	STOREN	zero, (t0)
	addi	t0, t0, ( __riscv_xlen / 8 )
	blt	t0, t1, 1b
	progress " .bss"

	/* Set up stack */
	la	sp, _estack
	progress " .stack"

	/* Store boot hart */
	la	t0, boot_hart
	STOREN	s0, (t0)

	/* Register device tree */
	la	a0, sysfdt
	mv	a1, s1
	li	a2, -1
	call	fdt_parse

	/* Call main program */
	progress "\n\n"
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

	/* File split information for the compressor */
	.section ".zinfo", "a", @progbits
	.ascii	"COPY"
	.word	0
	.word	_filesz
	.word	1
	.ascii	"BASE"
	.word	0
	.dword	_base
	.ascii	"ZREL"
	.word	_reloc_offset
	.word	_reloc_filesz
	.word	EM_RISCV
