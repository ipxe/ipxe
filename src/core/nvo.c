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

static size_t nvo_options_len ( struct nvs_options *nvo ) {
	struct dhcp_option *option;
	uint8_t sum;
	unsigned int i;
	size_t len;

	for ( sum = 0, i = 0 ; i < nvo->nvs->size ; i++ ) {
		sum += * ( ( uint8_t * ) ( nvo->options->data + i ) );
	}
	if ( sum != 0 ) {
		DBG ( "NVO %p has bad checksum %02x; assuming empty\n",
		      nvo, sum );
		return 0;
	}

	option = nvo->options->data;
	if ( option->tag == DHCP_PAD ) {
		DBG ( "NVO %p has bad start; assuming empty\n", nvo );
		return 0;
	}
	
	option = find_dhcp_option ( nvo->options, DHCP_END );
	if ( ! option ) {
		DBG ( "NVO %p has no end tag; assuming empty\n", nvo );
		return 0;
	}

	len = ( ( void * ) option - nvo->options->data + 1 );
	DBG ( "NVO %p contains %zd bytes of options (maximum %zd)\n",
	      nvo, len, nvo->nvs->size );

	return len;
}

int nvo_register ( struct nvs_options *nvo ) {
	struct dhcp_option *option;
	int rc;

	nvo->options = alloc_dhcp_options ( nvo->nvs->size );
	if ( ! nvo->options ) {
		DBG ( "NVO %p could not allocate %zd bytes\n",
		      nvo, nvo->nvs->size );
		rc = -ENOMEM;
		goto err;
	}

	if ( ( rc = nvo->nvs->read ( nvo->nvs, 0, nvo->options->data,
				     nvo->nvs->size ) ) != 0 ) {
		DBG ( "NVO %p could not read [0,%zd)\n",
		      nvo, nvo->nvs->size );
		goto err;
	}

	nvo->options->len = nvo->options->max_len;
	nvo->options->len = nvo_options_len ( nvo );
	if ( ! nvo->options->len ) {
		option = nvo->options->data;
		option->tag = DHCP_END;
		nvo->options->len = 1;
	}

	register_dhcp_options ( nvo->options );

	return 0;
	
 err:
	
	free_dhcp_options ( nvo->options );
	nvo->options = NULL;
	return rc;
}

void nvo_unregister ( struct nvs_options *nvo ) {
	if ( nvo->options ) {
		unregister_dhcp_options ( nvo->options );
		free_dhcp_options ( nvo->options );
		nvo->options = NULL;
	}
}
