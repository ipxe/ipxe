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
 * Key exchange self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <ipxe/crypto.h>
#include "exchange_test.h"

/**
 * Report a key exchange test result
 *
 * @v test		Key exchange test
 * @v file		Test code file
 * @v line		Test code line
 */
void exchange_okx ( struct exchange_test *test, const char *file,
		    unsigned int line ) {
	struct exchange_algorithm *exchange = test->exchange;
	uint8_t public[ exchange->pubsize ];
	uint8_t shared[ exchange->sharedsize ];
	int rc;

	/* Sanity checks */
	okx ( test->private_len == exchange->privsize, file, line );
	okx ( test->partner_len == exchange->pubsize, file, line );
	okx ( test->public_len == exchange->pubsize, file, line );
	okx ( ( ( test->shared_len == 0 ) ||
		( test->shared_len == exchange->sharedsize ) ), file, line );

	/* Verify calculation of public key */
	DBGC ( test, "KEX %s private key:\n", exchange->name );
	DBGC_HDA ( test, 0, test->private, exchange->privsize );
	exchange_public ( exchange, test->private, public );
	DBGC ( test, "KEX %s public key:\n", exchange->name );
	DBGC_HDA ( test, 0, public, exchange->pubsize );
	okx ( memcmp ( public, test->public, exchange->pubsize ) == 0,
	      file, line );

	/* Verify calculation of shared secret */
	DBGC ( test, "KEX %s partner key:\n", exchange->name );
	DBGC_HDA ( test, 0, test->partner, exchange->pubsize );
	rc = exchange_shared ( exchange, test->private, test->partner, shared );
	if ( test->shared_len ) {
		/* Verify successful calculation */
		okx ( rc == 0, file, line );
		DBGC ( test, "KEX %s shared secret:\n", exchange->name );
		DBGC_HDA ( test, 0, shared, exchange->sharedsize );
		okx ( memcmp ( shared, test->shared,
			       exchange->sharedsize ) == 0, file, line );
	} else {
		/* Verify failure */
		okx ( rc != 0, file, line );
	}
}
