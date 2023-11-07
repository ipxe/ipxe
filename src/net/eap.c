/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/eap.h>

/** @file
 *
 * Extensible Authentication Protocol
 *
 */

/**
 * Handle EAP Request-Identity
 *
 * @v supplicant	EAP supplicant
 * @ret rc		Return status code
 */
static int eap_rx_request_identity ( struct eap_supplicant *supplicant ) {
	struct net_device *netdev = supplicant->netdev;

	/* Treat Request-Identity as blocking the link */
	DBGC ( netdev, "EAP %s Request-Identity blocking link\n",
	       netdev->name );
	netdev_link_block ( netdev, EAP_BLOCK_TIMEOUT );

	/* Mark EAP as in progress */
	supplicant->flags |= EAP_FL_ONGOING;

	/* We have no identity to offer, so wait until the switch
	 * times out and switches to MAC Authentication Bypass (MAB).
	 */
	supplicant->flags |= EAP_FL_PASSIVE;

	return 0;
}

/**
 * Handle EAP Request
 *
 * @v supplicant	EAP supplicant
 * @v req		EAP request
 * @v len		Length of EAP request
 * @ret rc		Return status code
 */
static int eap_rx_request ( struct eap_supplicant *supplicant,
			    const struct eap_request *req, size_t len ) {
	struct net_device *netdev = supplicant->netdev;

	/* Sanity check */
	if ( len < sizeof ( *req ) ) {
		DBGC ( netdev, "EAP %s underlength request:\n", netdev->name );
		DBGC_HDA ( netdev, 0, req, len );
		return -EINVAL;
	}

	/* Handle according to type */
	switch ( req->type ) {
	case EAP_TYPE_IDENTITY:
		return eap_rx_request_identity ( supplicant );
	default:
		DBGC ( netdev, "EAP %s requested type %d unknown:\n",
		       netdev->name, req->type );
		DBGC_HDA ( netdev, 0, req, len );
		return -ENOTSUP;
	}
}

/**
 * Handle EAP Success
 *
 * @v supplicant	EAP supplicant
 * @ret rc		Return status code
 */
static int eap_rx_success ( struct eap_supplicant *supplicant ) {
	struct net_device *netdev = supplicant->netdev;

	/* Mark authentication as complete */
	supplicant->flags = EAP_FL_PASSIVE;

	/* Mark link as unblocked */
	DBGC ( netdev, "EAP %s Success\n", netdev->name );
	netdev_link_unblock ( netdev );

	return 0;
}

/**
 * Handle EAP Failure
 *
 * @v supplicant	EAP supplicant
 * @ret rc		Return status code
 */
static int eap_rx_failure ( struct eap_supplicant *supplicant ) {
	struct net_device *netdev = supplicant->netdev;

	/* Mark authentication as complete */
	supplicant->flags = EAP_FL_PASSIVE;

	/* Record error */
	DBGC ( netdev, "EAP %s Failure\n", netdev->name );
	return -EPERM;
}

/**
 * Handle EAP packet
 *
 * @v supplicant	EAP supplicant
 * @v data		EAP packet
 * @v len		Length of EAP packet
 * @ret rc		Return status code
 */
int eap_rx ( struct eap_supplicant *supplicant, const void *data,
	     size_t len ) {
	struct net_device *netdev = supplicant->netdev;
	const union eap_packet *eap = data;

	/* Sanity check */
	if ( len < sizeof ( eap->hdr ) ) {
		DBGC ( netdev, "EAP %s underlength header:\n", netdev->name );
		DBGC_HDA ( netdev, 0, eap, len );
		return -EINVAL;
	}

	/* Handle according to code */
	switch ( eap->hdr.code ) {
	case EAP_CODE_REQUEST:
		return eap_rx_request ( supplicant, &eap->req, len );
	case EAP_CODE_SUCCESS:
		return eap_rx_success ( supplicant );
	case EAP_CODE_FAILURE:
		return eap_rx_failure ( supplicant );
	default:
		DBGC ( netdev, "EAP %s unsupported code %d\n",
		       netdev->name, eap->hdr.code );
		DBGC_HDA ( netdev, 0, eap, len );
		return -ENOTSUP;
	}
}
