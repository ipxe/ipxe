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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * T-Head vendor extensions
 *
 */

#include <ipxe/sbi.h>
#include <ipxe/xthead.h>

/** Colour for debug messages */
#define colour THEAD_MVENDORID

/**
 * Check for a T-Head feature via SXSTATUS register
 *
 * @v feature		Feature bit
 * @ret supported	Feature is supported
 */
int xthead_supported ( unsigned long feature ) {
	struct sbi_return ret;
	unsigned long sxstatus;

	/* Check for a T-Head CPU */
	ret = sbi_ecall_0 ( SBI_BASE, SBI_BASE_MVENDORID );
	if ( ret.error )
		return 0;
	if ( ret.value != THEAD_MVENDORID ) {
		DBGC ( colour, "THEAD vendor ID mismatch: %#08lx\n",
		       ret.value );
		return 0;
	}

	/* Read SXSTATUS CSR */
	__asm__ ( "csrr %0, %1"
		  : "=r" ( sxstatus ) : "i" ( THEAD_SXSTATUS ) );
	DBGC ( colour, "THEAD sxstatus %#08lx\n", sxstatus );

	/* Check feature bit */
	return ( !! ( sxstatus & feature ) );
}
