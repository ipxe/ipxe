/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * Alternatively, you may distribute this code in source or binary
 * form, with or without modification, provided that the following
 * conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the above disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the above
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * RBG mechanism
 *
 * This mechanism is designed to comply with ANS X9.82 Part 4 (April
 * 2011 Draft) Section 10.  This standard is unfortunately not freely
 * available.
 *
 * The chosen RBG design is that of a DRBG with a live entropy source
 * with no conditioning function.  Only a single security strength is
 * supported.  No seedfile is used since there may be no non-volatile
 * storage available.  The system UUID is used as the personalisation
 * string.
 */

#include <stdint.h>
#include <string.h>
#include <ipxe/init.h>
#include <ipxe/settings.h>
#include <ipxe/uuid.h>
#include <ipxe/crypto.h>
#include <ipxe/drbg.h>
#include <ipxe/rbg.h>

/** The RBG */
struct random_bit_generator rbg;

/**
 * Start up RBG
 *
 * @ret rc		Return status code
 *
 * This is the RBG_Startup function defined in ANS X9.82 Part 4 (April
 * 2011 Draft) Section 9.1.2.2.
 */
static int rbg_startup ( void ) {
	union uuid uuid;
	int len;
	int rc;

	/* Record that startup has been attempted (even if unsuccessful) */
	rbg.started = 1;

	/* Try to obtain system UUID for use as personalisation
	 * string, in accordance with ANS X9.82 Part 3-2007 Section
	 * 8.5.2.  If no UUID is available, proceed without a
	 * personalisation string.
	 */
	if ( ( len = fetch_uuid_setting ( NULL, &uuid_setting, &uuid ) ) < 0 ) {
		rc = len;
		DBGC ( &rbg, "RBG could not fetch personalisation string: "
		       "%s\n", strerror ( rc ) );
		len = 0;
	}

	/* Instantiate DRBG */
	if ( ( rc = drbg_instantiate ( &rbg.state, &uuid, len ) ) != 0 ) {
		DBGC ( &rbg, "RBG could not instantiate DRBG: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Generate bits using RBG
 *
 * @v additional	Additional input
 * @v additional_len	Length of additional input
 * @v prediction_resist	Prediction resistance is required
 * @v data		Output buffer
 * @v len		Length of output buffer
 * @ret rc		Return status code
 *
 * This is the RBG_Generate function defined in ANS X9.82 Part 4
 * (April 2011 Draft) Section 9.1.2.2.
 */
int rbg_generate ( const void *additional, size_t additional_len,
		   int prediction_resist, void *data, size_t len ) {

	/* Attempt startup, if not already attempted */
	if ( ! rbg.started )
		rbg_startup();

	/* Generate bits.  The DRBG will itself return an error if it
	 * is not valid (e.g. due to an instantiation failure).
	 */
	return drbg_generate ( &rbg.state, additional, additional_len,
			       prediction_resist, data, len );
}

/**
 * Shut down RBG
 *
 */
static void rbg_shutdown ( void ) {

	/* Uninstantiate DRBG */
	drbg_uninstantiate ( &rbg.state );

	/* Clear startup attempted flag */
	rbg.started = 0;
}

/** RBG startup function */
static void rbg_startup_fn ( void ) {

	/* Start up RBG (if not already started on demand).  There is
	 * no way to report an error at this stage, but a failed
	 * startup will result in an invalid DRBG that refuses to
	 * generate bits.
	 */
	if ( ! rbg.started )
		rbg_startup();
}

/** RBG shutdown function */
static void rbg_shutdown_fn ( int booting __unused ) {

	/* Shut down RBG */
	rbg_shutdown();
}

/** RBG startup table entry */
struct startup_fn startup_rbg __startup_fn ( STARTUP_NORMAL ) = {
	.name = "rbg",
	.startup = rbg_startup_fn,
	.shutdown = rbg_shutdown_fn,
};
