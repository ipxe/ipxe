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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Perform Random Number Operation (PRNO) entropy source
 *
 */

#include <errno.h>
#include <ipxe/entropy.h>
#include <ipxe/facility.h>
#include <ipxe/prno.h>

struct entropy_source prno_entropy __entropy_source ( ENTROPY_PREFERRED );

/** Colour for debug messages */
#define colour &prno_entropy

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int prno_entropy_enable ( void ) {
	union prno_parameters params;
	struct s390x_scalar_pair dummy1;
	struct s390x_scalar_pair dummy2;
	register unsigned long fn asm ( "0" );
	register void *pp asm ( "1" );

	/* Check if PRNO is supported */
	if ( ! facility_is_installed ( FACILITY_MSA5 ) ) {
		DBGC ( colour, "PRNO instruction is not supported\n" );
		return -ENOTSUP;
	}
	DBGC ( colour, "PRNO instruction is supported\n" );

	/* Check if TRNG is supported */
	__asm__ ( ".machine push\n\t"
		  ".machine \"z14\"\n\t"
		  "prno %2, %3\n\t"
		  ".machine pop\n\t"
		  : "=r" ( fn ),
		    "=r" ( pp ),
		    "=a" ( dummy1 ),
		    "=a" ( dummy2 ),
		    "=m" ( params )
		  : "0" ( PRNO_FN_QUERY ),
		    "1" ( &params ) );
	if ( ! prno_is_supported ( &params.supported, PRNO_FN_TRNG ) ) {
		DBGC ( colour, "PRNO does not support TRNG (%016llx:%016llx)\n",
		       params.supported.mask[0], params.supported.mask[1] );
		return -ENOTSUP;
	}
	DBGC ( colour, "PRNO has support for TRNG\n" );

	/* Linux assumes that the TRNG produces 100% entropy.  Nothing
	 * seems to actually guarantee this, and we don't need much
	 * entropy, so assume conservatively that each byte contains
	 * at least one bit of min-entropy.
	 */
	entropy_init ( &prno_entropy, MIN_ENTROPY ( 1.0 ) );

	return 0;
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int prno_get_noise ( noise_sample_t *noise ) {
	struct s390x_pointer_pair raw = { noise, sizeof ( *noise ) };
	struct s390x_pointer_pair conditioned = { NULL, 0 };
	register unsigned long fn asm ( "0" );
	register void *pp asm ( "1" );

	/* Generate raw true random numbers */
	__asm__ ( ".machine push\n\t"
		  ".machine \"z14\"\n\t"
		  "\n1:\n\t"
		  "prno %2, %3\n\t"
		  "jo 1b\n\t"
		  ".machine pop\n\t"
		  : "=r" ( fn ),
		    "=r" ( pp ),
		    "+a" ( raw ),
		    "+a" ( conditioned ),
		    "=m" ( *noise )
		  : "0" ( PRNO_FN_TRNG ) );

	return 0;
}

/** Hardware entropy source */
struct entropy_source prno_entropy __entropy_source ( ENTROPY_PREFERRED ) = {
	.name = "prno",
	.enable = prno_entropy_enable,
	.get_noise = prno_get_noise,
};
