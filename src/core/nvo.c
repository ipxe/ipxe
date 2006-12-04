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

#include <stdint.h>
#include <string.h>
#include <gpxe/dhcp.h>
#include <gpxe/nvs.h>
#include <gpxe/nvo.h>

/** @file
 *
 * Non-volatile stored options
 *
 */

/**
 * Calculate total length of non-volatile stored options
 *
 * @v nvo		Non-volatile options block
 * @ret total_len	Total length of all fragments
 */
static size_t nvo_total_len ( struct nvo_block *nvo ) {
	struct nvo_fragment *fragment = nvo->fragments;
	size_t total_len = 0;

	for ( ; fragment->len ; fragment++ ) {
		total_len += fragment->len;
	}

	return total_len;
}

/**
 * Read non-volatile stored options from non-volatile storage device
 *
 * @v nvo		Non-volatile options block
 * @ret rc		Return status code
 */
static int nvo_read ( struct nvo_block *nvo ) {
	struct nvo_fragment *fragment = nvo->fragments;
	void *data = nvo->options->data;
	int rc;

	for ( ; fragment->len ; fragment++ ) {
		if ( ( rc = nvs_read ( nvo->nvs, fragment->address,
				       data, fragment->len ) ) != 0 ) {
			DBG ( "NVO %p could not read %zd bytes at %#04x\n",
			      nvo, fragment->len, fragment->address );
			return rc;
		}
		data += fragment->len;
	}
	
	return 0;
}

/**
 * Parse stored options
 *
 * @v nvo		Non-volatile options block
 * @v total_len		Total length of options data
 *
 * Verifies that the options data is valid, and configures the DHCP
 * options block.  If the data is not valid, it is replaced with an
 * empty options block.
 */
static void nvo_init_dhcp ( struct nvo_block *nvo, size_t total_len ) {
	struct dhcp_option_block *options = nvo->options;
	struct dhcp_option *option;
	uint8_t sum;
	unsigned int i;

	/* Steal one byte for the checksum */
	options->max_len = ( total_len - 1 );

	/* Verify checksum over whole block */
	for ( sum = 0, i = 0 ; i < total_len ; i++ ) {
		sum += * ( ( uint8_t * ) ( options->data + i ) );
	}
	if ( sum != 0 ) {
		DBG ( "NVO %p has bad checksum %02x; assuming empty\n",
		      nvo, sum );
		goto empty;
	}

	/* Check that we don't just have a block full of zeroes */
	option = options->data;
	if ( option->tag == DHCP_PAD ) {
		DBG ( "NVO %p has bad start; assuming empty\n", nvo );
		goto empty;
	}
	
	/* Search for the DHCP_END tag */
	options->len = options->max_len;
	option = find_dhcp_option ( options, DHCP_END );
	if ( ! option ) {
		DBG ( "NVO %p has no end tag; assuming empty\n", nvo );
		goto empty;
	}

	/* Set correct length of DHCP options */
	options->len = ( ( void * ) option - options->data + 1 );
	DBG ( "NVO %p contains %zd bytes of options (maximum %zd)\n",
	      nvo, options->len, options->max_len );
	return;

 empty:
	/* No options found; initialise an empty options block */
	option = options->data;
	option->tag = DHCP_END;
	options->len = 1;
	return;
}

/**
 * Register non-volatile stored options
 *
 * @v nvo		Non-volatile options block
 * @ret rc		Return status code
 */
int nvo_register ( struct nvo_block *nvo ) {
	size_t total_len;
	int rc;

	/* Allocate memory for options and read in from NVS */
	total_len = nvo_total_len ( nvo );
	nvo->options = alloc_dhcp_options ( total_len );
	if ( ! nvo->options ) {
		DBG ( "NVO %p could not allocate %zd bytes\n",
		      nvo, total_len );
		rc = -ENOMEM;
		goto err;
	}

	if ( ( rc = nvo_read ( nvo ) ) != 0 )
		goto err;

	/* Verify and register options */
	nvo_init_dhcp ( nvo, total_len );
	register_dhcp_options ( nvo->options );

	return 0;
	
 err:
	free_dhcp_options ( nvo->options );
	nvo->options = NULL;
	return rc;
}

/**
 * Unregister non-volatile stored options
 *
 * @v nvo		Non-volatile options block
 */
void nvo_unregister ( struct nvo_block *nvo ) {
	if ( nvo->options ) {
		unregister_dhcp_options ( nvo->options );
		free_dhcp_options ( nvo->options );
		nvo->options = NULL;
	}
}
