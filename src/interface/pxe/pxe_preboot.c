/** @file
 *
 * PXE Preboot API
 *
 */

/* PXE API interface for Etherboot.
 *
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdlib.h>
#include <gpxe/uaccess.h>
#include <gpxe/dhcp.h>
#include <gpxe/device.h>
#include <gpxe/netdevice.h>
#include <gpxe/isapnp.h>
#include <gpxe/init.h>
#include <basemem_packet.h>
#include "pxe.h"
#include "pxe_call.h"

/** Filename used for last TFTP request
 *
 * This is a bug-for-bug compatibility hack needed in order to work
 * with Microsoft Remote Installation Services (RIS).  The filename
 * used in a call to PXENV_RESTART_TFTP must be returned as the DHCP
 * filename in subsequent calls to PXENV_GET_CACHED_INFO.
 */
static char *pxe_ris_filename = NULL;

/* Avoid dragging in isapnp.o unnecessarily */
uint16_t isapnp_read_port;

/**
 * UNLOAD BASE CODE STACK
 *
 * @v None				-
 * @ret ...
 *
 */
PXENV_EXIT_t pxenv_unload_stack ( struct s_PXENV_UNLOAD_STACK *unload_stack ) {
	DBG ( "PXENV_UNLOAD_STACK" );

	unload_stack->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_GET_CACHED_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_get_cached_info ( struct s_PXENV_GET_CACHED_INFO
				     *get_cached_info ) {
	struct dhcp_packet dhcppkt;
	int ( * dhcp_packet_creator ) ( struct net_device *, int,
					struct dhcp_option_block *, void *,
					size_t, struct dhcp_packet * );
	unsigned int msgtype;
	void *data = NULL;
	size_t len;
	userptr_t buffer;
	int rc;

	DBG ( "PXENV_GET_CACHED_INFO %d", get_cached_info->PacketType );

	DBG ( " to %04x:%04x+%x", get_cached_info->Buffer.segment,
	      get_cached_info->Buffer.offset, get_cached_info->BufferSize );

	/* This is really, really awkward to support with our multiple
	 * sources of options.
	 */
	len = get_cached_info->BufferSize;
	if ( len == 0 ) {
		len = sizeof ( basemem_packet );
		get_cached_info->Buffer.segment = rm_ds;
		get_cached_info->Buffer.offset =
			( unsigned int ) ( & __from_data16 ( basemem_packet ) );
		get_cached_info->BufferLimit = len;
	}

	/* Allocate space for temporary copy */
	data = malloc ( len );
	if ( ! data ) {
		DBG ( " out of memory" );
		goto err;
	}

	/* Construct DHCP packet */
	if ( get_cached_info->PacketType == PXENV_PACKET_TYPE_DHCP_DISCOVER ) {
		dhcp_packet_creator = create_dhcp_request;
		msgtype = DHCPDISCOVER;
	} else {
		dhcp_packet_creator = create_dhcp_response;
		msgtype = DHCPACK;
	}
	if ( ( rc = dhcp_packet_creator ( pxe_netdev, msgtype, NULL,
					  data, len, &dhcppkt ) ) != 0 ) {
		DBG ( " failed to build packet" );
		goto err;
	}

	/* Overwrite filename to work around Microsoft RIS bug */
	if ( pxe_ris_filename ) {
		strncpy ( dhcppkt.dhcphdr->file, pxe_ris_filename,
			  sizeof ( dhcppkt.dhcphdr->file ) );
	}

	/* Copy packet to client buffer */
	buffer = real_to_user ( get_cached_info->Buffer.segment,
				get_cached_info->Buffer.offset );
	len = dhcppkt.len;
	copy_to_user ( buffer, 0, data, len );
	get_cached_info->BufferSize = len;

	free ( data );
	get_cached_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;

 err:
	if ( data )
		free ( data );
	get_cached_info->Status = PXENV_STATUS_OUT_OF_RESOURCES;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_RESTART_TFTP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_restart_tftp ( struct s_PXENV_TFTP_READ_FILE
				  *restart_tftp ) {
	PXENV_EXIT_t tftp_exit;

	DBG ( "PXENV_RESTART_TFTP " );

	/* Work around Microsoft RIS bug */
	free ( pxe_ris_filename );
	pxe_ris_filename = strdup ( ( char * ) restart_tftp->FileName );
	if ( ! pxe_ris_filename ) {
		restart_tftp->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}

	/* Words cannot describe the complete mismatch between the PXE
	 * specification and any possible version of reality...
	 */
	restart_tftp->Buffer = PXE_LOAD_PHYS; /* Fixed by spec, apparently */
	restart_tftp->BufferSize = ( 0xa0000 - PXE_LOAD_PHYS ); /* Near enough */
	tftp_exit = pxenv_tftp_read_file ( restart_tftp );
	if ( tftp_exit != PXENV_EXIT_SUCCESS )
		return tftp_exit;

	/* Fire up the new NBP */
	restart_tftp->Status = pxe_start_nbp();

	/* Not sure what "SUCCESS" actually means, since we can only
	 * return if the new NBP failed to boot...
	 */
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_START_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_start_undi ( struct s_PXENV_START_UNDI *start_undi ) {
	unsigned int bus_type;
	unsigned int location;
	struct net_device *netdev;

	DBG ( "PXENV_START_UNDI %04x:%04x:%04x",
	      start_undi->AX, start_undi->BX, start_undi->DX );

	/* Determine bus type and location.  Use a heuristic to decide
	 * whether we are PCI or ISAPnP
	 */
	if ( ( start_undi->DX >= ISAPNP_READ_PORT_MIN ) &&
	     ( start_undi->DX <= ISAPNP_READ_PORT_MAX ) &&
	     ( start_undi->BX >= ISAPNP_CSN_MIN ) &&
	     ( start_undi->BX <= ISAPNP_CSN_MAX ) ) {
		bus_type = BUS_TYPE_ISAPNP;
		location = start_undi->BX;
		/* Record ISAPnP read port for use by isapnp.c */
		isapnp_read_port = start_undi->DX;
	} else {
		bus_type = BUS_TYPE_PCI;
		location = start_undi->AX;
	}

	/* Probe for devices, etc. */
	startup();

	/* Look for a matching net device */
	netdev = find_netdev_by_location ( bus_type, location );
	if ( ! netdev ) {
		DBG ( " no net device found" );
		start_undi->Status = PXENV_STATUS_UNDI_CANNOT_INITIALIZE_NIC;
		return PXENV_EXIT_FAILURE;
	}
	DBG ( " using netdev %s", netdev->name );

	/* Save as PXE net device */
	pxe_set_netdev ( netdev );

	/* Hook INT 1A */
	pxe_hook_int1a();

	start_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_STOP_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_undi ( struct s_PXENV_STOP_UNDI *stop_undi ) {
	DBG ( "PXENV_STOP_UNDI" );

	/* Unhook INT 1A */
	pxe_unhook_int1a();

	/* Clear PXE net device */
	pxe_set_netdev ( NULL );

	/* Prepare for unload */
	shutdown();

	stop_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_START_BASE
 *
 * Status: won't implement (requires major structural changes)
 */
PXENV_EXIT_t pxenv_start_base ( struct s_PXENV_START_BASE *start_base ) {
	DBG ( "PXENV_START_BASE" );

	start_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_BASE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_base ( struct s_PXENV_STOP_BASE *stop_base ) {
	DBG ( "PXENV_STOP_BASE" );

	/* The only time we will be called is when the NBP is trying
	 * to shut down the PXE stack.  There's nothing we need to do
	 * in this call.
	 */

	stop_base->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
