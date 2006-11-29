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

/** @file
 *
 * Non-volatile storage
 *
 */

static size_t nvs_options_len ( struct nvs_device *nvs ) {
	struct dhcp_option *option;
	uint8_t sum;
	unsigned int i;
	size_t len;

	for ( sum = 0, i = 0 ; i < nvs->len ; i++ ) {
		sum += * ( ( uint8_t * ) ( nvs->options->data + i ) );
	}
	if ( sum != 0 ) {
		DBG ( "NVS %p has bad checksum %02x; assuming empty\n",
		      nvs, sum );
		return 0;
	}

	option = nvs->options->data;
	if ( option->tag == DHCP_PAD ) {
		DBG ( "NVS %p has bad start; assuming empty\n", nvs );
		return 0;
	}
	
	option = find_dhcp_option ( nvs->options, DHCP_END );
	if ( ! option ) {
		DBG ( "NVS %p has no end tag; assuming empty\n", nvs );
		return 0;
	}

	len = ( ( void * ) option - nvs->options->data + 1 );
	DBG ( "NVS %p contains %zd bytes of options (maximum %zd)\n",
	      nvs, len, nvs->len );

	return len;
}

int nvs_register ( struct nvs_device *nvs ) {
	struct dhcp_option *option;
	int rc;

	nvs->options = alloc_dhcp_options ( nvs->len );
	if ( ! nvs->options ) {
		DBG ( "NVS %p could not allocate %zd bytes\n", nvs, nvs->len );
		rc = -ENOMEM;
		goto err;
	}

	if ( ( rc = nvs->op->read ( nvs, 0, nvs->options->data,
				    nvs->len ) ) != 0 ) {
		DBG ( "NVS %p could not read [0,%zd)\n", nvs, nvs->len );
		goto err;
	}

	nvs->options->len = nvs->options->max_len;
	nvs->options->len = nvs_options_len ( nvs );
	if ( ! nvs->options->len ) {
		option = nvs->options->data;
		option->tag = DHCP_END;
		nvs->options->len = 1;
	}

	register_dhcp_options ( nvs->options );

	return 0;
	
 err:
	
	free_dhcp_options ( nvs->options );
	nvs->options = NULL;
	return rc;
}

void nvs_unregister ( struct nvs_device *nvs ) {
	if ( nvs->options ) {
		unregister_dhcp_options ( nvs->options );
		free_dhcp_options ( nvs->options );
		nvs->options = NULL;
	}
}
