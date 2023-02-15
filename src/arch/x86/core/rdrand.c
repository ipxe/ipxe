/*
 * Copyright (C) 2023 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Hardware random number generator
 *
 */

#include <errno.h>
#include <ipxe/cpuid.h>
#include <ipxe/entropy.h>

/** Number of times to retry RDRAND instruction */
#define RDRAND_RETRY_COUNT 16

/** Colour for debug messages */
#define colour CPUID_FEATURES_INTEL_ECX_RDRAND

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int rdrand_entropy_enable ( void ) {
	struct x86_features features;

	/* Check that RDRAND is supported */
	x86_features ( &features );
	if ( ! ( features.intel.ecx & CPUID_FEATURES_INTEL_ECX_RDRAND ) ) {
		DBGC ( colour, "RDRAND not supported\n" );
		return -ENOTSUP;
	}

	return 0;
}

/**
 * Disable entropy gathering
 *
 */
static void rdrand_entropy_disable ( void ) {

	/* Nothing to do */
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int rdrand_get_noise ( noise_sample_t *noise ) {
	unsigned int result;
	unsigned int discard_c;
	unsigned int ok;

	/* Issue RDRAND, retrying until CF is set */
	__asm__ ( "\n1:\n\t"
		  "rdrand %0\n\t"
		  "sbb %1, %1\n\t"
		  "loopz 1b\n\t"
		  : "=r" ( result ), "=r" ( ok ), "=c" ( discard_c )
		  : "2" ( RDRAND_RETRY_COUNT ) );
	if ( ! ok ) {
		DBGC ( colour, "RDRAND failed to become ready\n" );
		return -EBUSY;
	}

	*noise = result;
	return 0;
}

PROVIDE_ENTROPY_INLINE ( rdrand, min_entropy_per_sample );
PROVIDE_ENTROPY ( rdrand, entropy_enable, rdrand_entropy_enable );
PROVIDE_ENTROPY ( rdrand, entropy_disable, rdrand_entropy_disable );
PROVIDE_ENTROPY ( rdrand, get_noise, rdrand_get_noise );
