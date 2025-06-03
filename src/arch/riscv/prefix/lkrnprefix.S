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
 * Linux kernel prefix
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

	/* Layout of kernel header */
	.struct	0
hdr_code0:		.space	4
hdr_code1:		.space	4
hdr_text_offset:	.space	8
hdr_image_size:		.space	8
hdr_flags:		.space	8
hdr_version:		.space	4
hdr_res1:		.space	4
hdr_res2:		.space	8
hdr_magic:		.space	8
hdr_magic2:		.space	4
hdr_res3:		.space	4
hdr_end:
	.org	64
	.previous

/* Header version */
#define HDR_VERSION( major, minor ) ( ( (major) << 16 ) | (minor) )
#define HDR_VERSION_0_2 HDR_VERSION ( 0, 2 )

/* Header flags */
#define HDR_FL_BIG_ENDIAN 0x00000001

/* Magic numbers */
#define HDR_MAGIC	"RISCV\0\0\0"
#define HDR_MAGIC2	"RSC\x05"

	/*
	 * Linux kernel header
	 */
	.section ".prefix", "ax", @progbits

	/* Executable code / MZ header (for EFI-compatible binaries) */
	.org	hdr_code0
	j	_lkrn_start

	/* Image load offset
	 *
	 * Must be set to the size of a single "megapage" (2MB for
	 * 64-bit, 4MB for 32-bit).
	 */
	.org	hdr_text_offset
	.dword	_max_align

	/* Image size (including uninitialised-data potions) */
	.org	hdr_image_size
	.dword	_memsz

	/* Flags */
	.org	hdr_flags
	.dword	0

	/* Version */
	.org	hdr_version
	.word	HDR_VERSION_0_2

	/* Magic numbers */
	.org	hdr_magic
	.ascii	HDR_MAGIC
	.org	hdr_magic2
	.ascii	HDR_MAGIC2

	.org	hdr_end

	/*
	 * Linux kernel entry point
	 */
	.globl	_lkrn_start
_lkrn_start:
	/* Identify temporary page table and stack space
	 *
	 * Linux expects to be placed at the image load offset from
	 * the start of RAM.  Assume that our loaded image is
	 * therefore already writable, and that we can therefore use
	 * the page table and stack within our (not yet zeroed) .bss
	 * section.
	 */
	la	a2, page_table
	la	sp, _estack

	/* Install iPXE */
	call	install

	/* Call main program */
	call	main

	/* We have no return path, since the Linux kernel does not
	 * define that a valid return address exists.
	 *
	 * Attempt a system reset, since there is nothing else we can
	 * viably do at this point.
	 */
	j	reset_system
	.size	_lkrn_start, . - _lkrn_start
