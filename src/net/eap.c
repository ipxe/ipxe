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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/eap.h>

/** @file
 *
 * Extensible Authentication Protocol
 *
 */

/**
 * Transmit EAP response
 *
 * @v supplicant	EAP supplicant
 * @v rsp		Response type data
 * @v rsp_len		Length of response type data
 * @ret rc		Return status code
 */
int eap_tx_response ( struct eap_supplicant *supplicant,
		      const void *rsp, size_t rsp_len ) {
	struct net_device *netdev = supplicant->netdev;
	struct eap_message *msg;
	size_t len;
	int rc;

	/* Allocate and populate response */
	len = ( sizeof ( *msg ) + rsp_len );
	msg = malloc ( len );
	if ( ! msg ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	msg->hdr.code = EAP_CODE_RESPONSE;
	msg->hdr.id = supplicant->id;
	msg->hdr.len = htons ( len );
	msg->type = supplicant->type;
	memcpy ( msg->data, rsp, rsp_len );
	DBGC ( netdev, "EAP %s Response id %#02x type %d\n",
	       netdev->name, msg->hdr.id, msg->type );

	/* Transmit response */
	if ( ( rc = supplicant->tx ( supplicant, msg, len ) ) != 0 ) {
		DBGC ( netdev, "EAP %s could not transmit: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_tx;
	}

 err_tx:
	free ( msg );
 err_alloc:
	return rc;
}

/**
 * Transmit EAP NAK
 *
 * @v supplicant	EAP supplicant
 * @ret rc		Return status code
 */
static int eap_tx_nak ( struct eap_supplicant *supplicant ) {
	struct net_device *netdev = supplicant->netdev;
	unsigned int max = table_num_entries ( EAP_METHODS );
	uint8_t methods[ max + 1 /* potential EAP_TYPE_NONE */ ];
	unsigned int count = 0;
	struct eap_method *method;

	/* Populate methods list */
	DBGC ( netdev, "EAP %s Nak offering types {", netdev->name );
	for_each_table_entry ( method, EAP_METHODS ) {
		if ( method->type > EAP_TYPE_NAK ) {
			DBGC ( netdev, "%s%d",
			       ( count ? ", " : "" ), method->type );
			methods[count++] = method->type;
		}
	}
	if ( ! count )
		methods[count++] = EAP_TYPE_NONE;
	DBGC ( netdev, "}\n" );
	assert ( count <= max );

	/* Transmit response */
	supplicant->type = EAP_TYPE_NAK;
	return eap_tx_response ( supplicant, methods, count );
}

/**
 * Handle EAP Request-Identity
 *
 * @v supplicant	EAP supplicant
 * @v req		Request type data
 * @v req_len		Length of request type data
 * @ret rc		Return status code
 */
static int eap_rx_identity ( struct eap_supplicant *supplicant,
			     const void *req, size_t req_len ) {
	struct net_device *netdev = supplicant->netdev;
	void *rsp;
	int rsp_len;
	int rc;

	/* Treat Request-Identity as blocking the link */
	DBGC ( netdev, "EAP %s Request-Identity blocking link\n",
	       netdev->name );
	DBGC_HDA ( netdev, 0, req, req_len );
	netdev_link_block ( netdev, EAP_BLOCK_TIMEOUT );

	/* Mark EAP as in progress */
	supplicant->flags |= EAP_FL_ONGOING;

	/* Construct response, if applicable */
	rsp_len = fetch_raw_setting_copy ( netdev_settings ( netdev ),
					   &username_setting, &rsp );
	if ( rsp_len < 0 ) {
		/* We have no identity to offer, so wait until the
		 * switch times out and switches to MAC Authentication
		 * Bypass (MAB).
		 */
		DBGC2 ( netdev, "EAP %s has no identity\n", netdev->name );
		supplicant->flags |= EAP_FL_PASSIVE;
		rc = 0;
		goto no_response;
	}

	/* Transmit response */
	if ( ( rc = eap_tx_response ( supplicant, rsp, rsp_len ) ) != 0 )
		goto err_tx;

 err_tx:
	free ( rsp );
 no_response:
	return rc;
}

/** EAP Request-Identity method */
struct eap_method eap_identity_method __eap_method = {
	.type = EAP_TYPE_IDENTITY,
	.rx = eap_rx_identity,
};

/**
 * Handle EAP Request
 *
 * @v supplicant	EAP supplicant
 * @v msg		EAP request
 * @v len		Length of EAP request
 * @ret rc		Return status code
 */
static int eap_rx_request ( struct eap_supplicant *supplicant,
			    const struct eap_message *msg, size_t len ) {
	struct net_device *netdev = supplicant->netdev;
	struct eap_method *method;
	const void *req;
	size_t req_len;

	/* Sanity checks */
	if ( len < sizeof ( *msg ) ) {
		DBGC ( netdev, "EAP %s underlength request:\n", netdev->name );
		DBGC_HDA ( netdev, 0, msg, len );
		return -EINVAL;
	}
	if ( len < ntohs ( msg->hdr.len ) ) {
		DBGC ( netdev, "EAP %s truncated request:\n", netdev->name );
		DBGC_HDA ( netdev, 0, msg, len );
		return -EINVAL;
	}
	req = msg->data;
	req_len = ( ntohs ( msg->hdr.len ) - sizeof ( *msg ) );

	/* Record request details */
	supplicant->id = msg->hdr.id;
	supplicant->type = msg->type;
	DBGC ( netdev, "EAP %s Request id %#02x type %d\n",
	       netdev->name, msg->hdr.id, msg->type );

	/* Handle according to type */
	for_each_table_entry ( method, EAP_METHODS ) {
		if ( msg->type == method->type )
			return method->rx ( supplicant, req, req_len );
	}
	DBGC ( netdev, "EAP %s requested type %d unknown:\n",
	       netdev->name, msg->type );
	DBGC_HDA ( netdev, 0, msg, len );

	/* Send NAK if applicable */
	if ( msg->type > EAP_TYPE_NAK )
		return eap_tx_nak ( supplicant );

	return -ENOTSUP;
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
		return eap_rx_request ( supplicant, &eap->msg, len );
	case EAP_CODE_RESPONSE:
		DBGC2 ( netdev, "EAP %s ignoring response\n", netdev->name );
		return 0;
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

/* Drag in objects via eap_rx() */
REQUIRING_SYMBOL ( eap_rx );

/* Drag in EAP configuration */
REQUIRE_OBJECT ( config_eap );
