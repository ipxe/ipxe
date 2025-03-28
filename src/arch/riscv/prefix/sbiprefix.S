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

/* SBI debug console extension */
#define SBI_DBCN ( ( 'D' << 24 ) | ( 'B' << 16 ) | ( 'C' << 8 ) | 'N' )
#define SBI_DBCN_WRITE 0x00

/* SBI system reset extension */
#define SBI_SRST ( ( 'S' << 24 ) | ( 'R' << 16 ) | ( 'S' << 8 ) | 'T' )
#define SBI_SRST_SYSTEM_RESET 0x00
#define SBI_RESET_COLD 0x00000001

/* Relative relocation type */
#define R_RISCV_RELATIVE 3

	/* Layout of a relocation record */
	.struct 0
rela_offset:	.space ( __riscv_xlen / 8 )
rela_type:	.space ( __riscv_xlen / 8 )
rela_addend:	.space ( __riscv_xlen / 8 )
rela_len:
	.previous

	/*
	 * Display progress message via debug console
	 */
	.macro	progress message
#ifndef NDEBUG
	.section ".prefix.data", "aw", @progbits
progress_\@:
	.ascii	"\message"
	.equ	progress_\@_len, . - progress_\@
	.size	progress_\@, . - progress_\@
	.previous
	li	a7, SBI_DBCN
	li	a6, SBI_DBCN_WRITE
	li	a0, progress_\@_len
	la	a1, progress_\@
	mv	a2, zero
	ecall
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
	la	t0, _reloc
	la	t1, _ereloc
	la	t2, _sbi_start
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
2:	/* Loop */
	addi	t0, t0, rela_len
	blt	t0, t1, 1b
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
	progress "\niPXE->SBI reset\n"
	li	a7, SBI_SRST
	li	a6, SBI_SRST_SYSTEM_RESET
	li	a0, SBI_RESET_COLD
	mv	a1, zero
	ecall

	/* If reset failed, lock the system */
	progress "(reset failed)\n"
1:	wfi
	j	1b
	.size	_sbi_start, . - _sbi_start

	/* File split information for the compressor */
	.section ".zinfo", "a", @progbits
	.ascii	"COPY"
	.word	0
	.word	_sbi_filesz
	.word	1
