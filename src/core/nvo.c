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
#include <errno.h>
#include <gpxe/dhcp.h>
#include <gpxe/nvs.h>
#include <gpxe/nvo.h>

/** @file
 *
 * Non-volatile stored options
 *
 */

#warning "Temporary hack"
struct nvo_block *ugly_nvo_hack = NULL;

/**
 * Calculate checksum over non-volatile stored options
 *
 * @v nvo		Non-volatile options block
 * @ret sum		Checksum
 */
static unsigned int nvo_checksum ( struct nvo_block *nvo ) {
	uint8_t *data = nvo->options->data;
	uint8_t sum = 0;
	unsigned int i;

	for ( i = 0 ; i < nvo->total_len ; i++ ) {
		sum += *(data++);
	}
	return sum;
}

/**
 * Load non-volatile stored options from non-volatile storage device
 *
 * @v nvo		Non-volatile options block
 * @ret rc		Return status code
 */
static int nvo_load ( struct nvo_block *nvo ) {
	void *data = nvo->options->data;
	struct nvo_fragment *fragment;
	int rc;

	/* Read data a fragment at a time */
	for ( fragment = nvo->fragments ; fragment->len ; fragment++ ) {
		if ( ( rc = nvs_read ( nvo->nvs, fragment->address,
				       data, fragment->len ) ) != 0 ) {
			DBG ( "NVO %p could not read %zd bytes at %#04x\n",
			      nvo, fragment->len, fragment->address );
			return rc;
		}
		data += fragment->len;
	}

	DBG ( "NVO %p loaded from non-volatile storage\n", nvo );
	return 0;
}

/**
 * Save non-volatile stored options back to non-volatile storage device
 *
 * @v nvo		Non-volatile options block
 * @ret rc		Return status code
 */
int nvo_save ( struct nvo_block *nvo ) {
	void *data = nvo->options->data;
	uint8_t *checksum = ( data + nvo->total_len - 1 );
	struct nvo_fragment *fragment;
	int rc;

	/* Recalculate checksum */
	*checksum -= nvo_checksum ( nvo );

	/* Write data a fragment at a time */
	for ( fragment = nvo->fragments ; fragment->len ; fragment++ ) {
		if ( ( rc = nvs_write ( nvo->nvs, fragment->address,
					data, fragment->len ) ) != 0 ) {
			DBG ( "NVO %p could not write %zd bytes at %#04x\n",
			      nvo, fragment->len, fragment->address );
			return rc;
		}
		data += fragment->len;
	}

	DBG ( "NVO %p saved to non-volatile storage\n", nvo );
	return 0;
}

/**
 * Parse stored options
 *
 * @v nvo		Non-volatile options block
 *
 * Verifies that the options data is valid, and configures the DHCP
 * options block.  If the data is not valid, it is replaced with an
 * empty options block.
 */
static void nvo_init_dhcp ( struct nvo_block *nvo ) {
	struct dhcp_option_block *options = nvo->options;
	struct dhcp_option *option;

	/* Steal one byte for the checksum */
	options->max_len = ( nvo->total_len - 1 );

	/* Verify checksum over whole block */
	if ( nvo_checksum ( nvo ) != 0 ) {
		DBG ( "NVO %p has bad checksum %02x; assuming empty\n",
		      nvo, nvo_checksum ( nvo ) );
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
	struct nvo_fragment *fragment = nvo->fragments;
	int rc;

	/* Calculate total length of all fragments */
	nvo->total_len = 0;
	for ( fragment = nvo->fragments ; fragment->len ; fragment++ ) {
		nvo->total_len += fragment->len;
	}

	/* Allocate memory for options and read in from NVS */
	nvo->options = alloc_dhcp_options ( nvo->total_len );
	if ( ! nvo->options ) {
		DBG ( "NVO %p could not allocate %zd bytes\n",
		      nvo, nvo->total_len );
		rc = -ENOMEM;
		goto err;
	}
	if ( ( rc = nvo_load ( nvo ) ) != 0 )
		goto err;

	/* Verify and register options */
	nvo_init_dhcp ( nvo );
	register_dhcp_options ( nvo->options );

	ugly_nvo_hack = nvo;

	DBG ( "NVO %p registered\n", nvo );
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

	DBG ( "NVO %p unregistered\n", nvo );

	ugly_nvo_hack = NULL;
}
