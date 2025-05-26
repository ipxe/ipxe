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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Entropy source extension (Zkr)
 *
 */

#include <errno.h>
#include <ipxe/csr.h>
#include <ipxe/entropy.h>
#include <ipxe/drbg.h>

struct entropy_source zkr_entropy __entropy_source ( ENTROPY_PREFERRED );

/** Seed CSR operational state */
#define ZKR_SEED_OPST_MASK 0xc0000000UL
#define ZKR_SEED_OPST_ES16 0x80000000UL /**< 16 bits of entropy available */

/** Number of times to retry reading from seed CSR */
#define ZKR_SEED_MAX_RETRY 1024

/** Colour for debug messages */
#define colour &zkr_entropy

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int zkr_entropy_enable ( void ) {

	/* Check if seed CSR is accessible in S-mode */
	if ( ! csr_can_write ( "seed", 0 ) ) {
		DBGC ( colour, "ZKR cannot access seed CSR\n" );
		return -ENOTSUP;
	}

	/* RISC-V ISA mandates that 128 bits of full entropy shall be
	 * obtained from 256 entropy bits read from the seed CSR.
	 *
	 * Each 16-bit sample therefore contains 8 bits of
	 * min-entropy.
	 */
	entropy_init ( &zkr_entropy, MIN_ENTROPY ( 8.0 ) );

	return 0;
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int zkr_get_noise ( noise_sample_t *noise ) {
	unsigned long seed;
	unsigned int i;

	/* Read entropy from seed CSR */
	for ( i = 0 ; i < ZKR_SEED_MAX_RETRY ; i++ ) {

		/* Read seed CSR */
		__asm__ __volatile__ ( "csrrw %0, seed, zero" :
				       "=r" ( seed ) );

		/* Check operationsl state */
		if ( ( seed & ZKR_SEED_OPST_MASK ) == ZKR_SEED_OPST_ES16 ) {

			/* Return entropy from both halves of the
			 * 16-bit entropy source value.
			 */
			*noise = ( seed ^ ( seed >> 8 ) );
			return 0;
		}
	}

	DBGC ( colour, "ZKR could not source entropy (seed %#08lx)\n", seed );
	return -EBUSY;
}

/** Hardware entropy source */
struct entropy_source zkr_entropy __entropy_source ( ENTROPY_PREFERRED ) = {
	.name = "zkr",
	.enable = zkr_entropy_enable,
	.get_noise = zkr_get_noise,
};
