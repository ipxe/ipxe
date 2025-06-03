/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <errno.h>
#include <ipxe/entropy.h>
#include <ipxe/crc32.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/Rng.h>

/** @file
 *
 * EFI random number generator protocol entropy source
 *
 */

struct entropy_source efirng_entropy __entropy_source ( ENTROPY_NORMAL );

/** Random number generator protocol */
static EFI_RNG_PROTOCOL *efirng;
EFI_REQUEST_PROTOCOL ( EFI_RNG_PROTOCOL, &efirng );

/** Minimum number of bytes to request from RNG
 *
 * The UEFI spec states (for no apparently good reason) that "When a
 * Deterministic Random Bit Generator (DRBG) is used on the output of
 * a (raw) entropy source, its security level must be at least 256
 * bits."  The EDK2 codebase (mis)interprets this to mean that the
 * call to GetRNG() should fail if given a buffer less than 32 bytes.
 *
 * Incidentally, nothing in the EFI RNG protocol provides any way to
 * report the actual amount of entropy returned by GetRNG().
 */
#define EFIRNG_LEN 32

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int efirng_enable ( void ) {

	/* Check for RNG protocol support */
	if ( ! efirng ) {
		DBGC ( &efirng, "EFIRNG has no RNG protocol\n" );
		return -ENOTSUP;
	}

	/* Nothing in the EFI specification provides any clue as to
	 * how much entropy will be returned by GetRNG().  Make a
	 * totally uninformed (and conservative guess) that each
	 * sample will contain at least one bit of entropy.
	 */
	entropy_init ( &efirng_entropy, MIN_ENTROPY ( 1.0 ) );

	return 0;
}

/**
 * Get noise sample from RNG protocol
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int efirng_get_noise ( noise_sample_t *noise ) {
	uint8_t buf[EFIRNG_LEN];
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	assert ( efirng != NULL );

	/* Get the minimum allowed number of random bytes */
	if ( ( efirc = efirng->GetRNG ( efirng, NULL, sizeof ( buf ),
					buf ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efirng, "ENTROPY could not read from RNG: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Reduce random bytes to a single noise sample.  This seems
	 * like overkill, but we have no way of knowing how much
	 * entropy is actually present in the bytes returned by the
	 * RNG protocol.
	 */
	*noise = crc32_le ( 0, buf, sizeof ( buf ) );

	return 0;
}

/** EFI random number generator protocol entropy source */
struct entropy_source efirng_entropy __entropy_source ( ENTROPY_NORMAL ) = {
	.name = "efirng",
	.enable = efirng_enable,
	.get_noise = efirng_get_noise,
};
