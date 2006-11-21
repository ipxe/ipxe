/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <malloc.h>
#include <gpxe/crypto.h>
#include <gpxe/chap.h>

/** @file
 *
 * CHAP protocol
 *
 */

/**
 * Initialise CHAP challenge/response
 *
 * @v chap		CHAP challenge/response
 * @v digest		Digest algorithm to use
 * @ret rc		Return status code
 *
 * Initialises a CHAP challenge/response structure.  This routine
 * allocates memory, and so may fail.  The allocated memory must
 * eventually be freed by a call to chap_finish().
 */
int chap_init ( struct chap_challenge *chap,
		struct digest_algorithm *digest ) {
	assert ( chap->digest == NULL );
	assert ( chap->digest_context == NULL );
	assert ( chap->response == NULL );

	chap->digest = digest;
	chap->digest_context = malloc ( digest->context_len );
	if ( ! chap->digest_context )
		goto err;
	chap->response = malloc ( digest->digest_len );
	if ( ! chap->response )
		goto err;
	chap->response_len = digest->digest_len;
	chap->digest->init ( chap->digest_context );
	return 0;

 err:
	chap_finish ( chap );
	return -ENOMEM;
}

/**
 * Add data to the CHAP challenge
 *
 * @v chap		CHAP challenge/response
 * @v data		Data to add
 * @v len		Length of data to add
 */
void chap_update ( struct chap_challenge *chap, const void *data,
		   size_t len ) {
	assert ( chap->digest != NULL );
	assert ( chap->digest_context != NULL );

	chap->digest->update ( chap->digest_context, data, len );
}

/**
 * Respond to the CHAP challenge
 *
 * @v chap		CHAP challenge/response
 *
 * Calculates the final CHAP response value, and places it in @c
 * chap->response, with a length of @c chap->response_len.
 */
void chap_respond ( struct chap_challenge *chap ) {
	assert ( chap->digest != NULL );
	assert ( chap->digest_context != NULL );
	assert ( chap->response != NULL );

	chap->digest->finish ( chap->digest_context, chap->response );
}

/**
 * Free resources used by a CHAP challenge/response
 *
 * @v chap		CHAP challenge/response
 */
void chap_finish ( struct chap_challenge *chap ) {
	free ( chap->digest_context );
	chap->digest_context = NULL;
	free ( chap->response );
	chap->response = NULL;
	chap->digest = NULL;
}
