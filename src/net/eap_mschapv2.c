/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/mschapv2.h>
#include <ipxe/eap.h>

/** @file
 *
 * EAP MS-CHAPv2 authentication method
 *
 * EAP-MSCHAPv2 was described in a draft RFC first published in 2002
 * (draft-kamath-pppext-eap-mschapv2-02.txt).  The draft eventually
 * expired in 2007 without becoming an official RFC, quite possibly
 * because the protocol design was too ugly to be called an IETF
 * standard.  It is, however, fairly widely used.
 */

/** An EAP MS-CHAPv2 request message */
struct eap_mschapv2_request {
	/** EAP-MSCHAPv2 header */
	struct eap_mschapv2 hdr;
	/** MS-CHAPv2 challenge length (fixed value) */
	uint8_t len;
	/** MS-CHAPv2 challenge */
	struct mschapv2_challenge msg;
} __attribute__ (( packed ));

/** An EAP MS-CHAPv2 response message */
struct eap_mschapv2_response {
	/** EAP-MSCHAPv2 header */
	struct eap_mschapv2 hdr;
	/** MS-CHAPv2 response length (fixed value) */
	uint8_t len;
	/** MS-CHAPv2 response */
	struct mschapv2_response msg;
	/** User name */
	char name[0];
} __attribute__ (( packed ));

/** An EAP MS-CHAPv2 success request message */
struct eap_mschapv2_success_request {
	/** EAP-MSCHAPv2 header */
	struct eap_mschapv2 hdr;
	/** Message */
	char message[0];
} __attribute__ (( packed ));

/** An EAP MS-CHAPv2 success response message */
struct eap_mschapv2_success_response {
	/** Opcode */
	uint8_t code;
} __attribute__ (( packed ));

/**
 * Handle EAP MS-CHAPv2 request
 *
 * @v supplicant	EAP supplicant
 * @v hdr		EAP-MSCHAPv2 header
 * @v len		Message length
 * @ret rc		Return status code
 */
static int eap_rx_mschapv2_request ( struct eap_supplicant *supplicant,
				     const struct eap_mschapv2 *hdr,
				     size_t len ) {
	struct net_device *netdev = supplicant->netdev;
	struct settings *settings = netdev_settings ( netdev );
	const struct eap_mschapv2_request *msreq =
		container_of ( hdr, struct eap_mschapv2_request, hdr );
	struct eap_mschapv2_response *msrsp;
	struct mschapv2_challenge peer;
	char *username;
	char *password;
	int username_len;
	int password_len;
	size_t msrsp_len;
	unsigned int i;
	int rc;

	/* Sanity check */
	if ( len < sizeof ( *msreq ) ) {
		DBGC ( netdev, "EAP %s underlength MS-CHAPv2 request\n",
		       netdev->name );
		DBGC_HDA ( netdev, 0, hdr, len );
		rc = -EINVAL;
		goto err_sanity;
	}

	/* Fetch username and password */
	username_len = fetch_string_setting_copy ( settings, &username_setting,
						   &username );
	if ( username_len < 0 ) {
		rc = username_len;
		DBGC ( netdev, "EAP %s has no username: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_username;
	}
	password_len = fetch_string_setting_copy ( settings, &password_setting,
						   &password );
	if ( password_len < 0 ) {
		rc = password_len;
		DBGC ( netdev, "EAP %s has no password: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_password;
	}

	/* Construct a peer challenge.  We do not perform mutual
	 * authentication, so this does not need to be strong.
	 */
	for ( i = 0 ; i < ( sizeof ( peer.byte ) /
			    sizeof ( peer.byte[0] ) ) ; i++ ) {
		peer.byte[i] = random();
	}

	/* Allocate response */
	msrsp_len = ( sizeof ( *msrsp ) + username_len );
	msrsp = malloc ( msrsp_len );
	if ( ! msrsp ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Construct response */
	msrsp->hdr.code = EAP_CODE_RESPONSE;
	msrsp->hdr.id = msreq->hdr.id;
	msrsp->hdr.len = htons ( msrsp_len );
	msrsp->len = sizeof ( msrsp->msg );
	mschapv2_response ( username, password, &msreq->msg, &peer,
			    &msrsp->msg );
	memcpy ( msrsp->name, username, username_len );

	/* Send response */
	if ( ( rc = eap_tx_response ( supplicant, msrsp, msrsp_len ) ) != 0 )
		goto err_tx;

 err_tx:
	free ( msrsp );
 err_alloc:
	free ( password );
 err_password:
	free ( username );
 err_username:
 err_sanity:
	return rc;
}

/**
 * Handle EAP MS-CHAPv2 success request
 *
 * @v supplicant	EAP supplicant
 * @v hdr		EAP-MSCHAPv2 header
 * @v len		Message length
 * @ret rc		Return status code
 */
static int eap_rx_mschapv2_success ( struct eap_supplicant *supplicant,
				     const struct eap_mschapv2 *hdr,
				     size_t len ) {
	const struct eap_mschapv2_success_request *msreq =
		container_of ( hdr, struct eap_mschapv2_success_request, hdr );
	static const struct eap_mschapv2_success_response msrsp = {
		.code = EAP_CODE_SUCCESS,
	};

	/* Sanity check */
	assert ( len >= sizeof ( *msreq ) );

	/* The success request contains the MS-CHAPv2 authenticator
	 * response, which could potentially be used to verify that
	 * the EAP authenticator also knew the password (or, at least,
	 * the MD4 hash of the password).
	 *
	 * Our model for EAP does not encompass mutual authentication:
	 * we will starting sending plaintext packets (e.g. DHCP
	 * requests) over the link even before EAP completes, and our
	 * only use for an EAP success is to mark the link as
	 * unblocked.
	 *
	 * We therefore ignore the content of the success request and
	 * just send back a success response, so that the EAP
	 * authenticator will complete the process and send through
	 * the real EAP success packet (which will, in turn, cause us
	 * to unblock the link).
	 */
	return eap_tx_response ( supplicant, &msrsp, sizeof ( msrsp ) );
}

/**
 * Handle EAP MS-CHAPv2
 *
 * @v supplicant	EAP supplicant
 * @v req		Request type data
 * @v req_len		Length of request type data
 * @ret rc		Return status code
 */
static int eap_rx_mschapv2 ( struct eap_supplicant *supplicant,
			     const void *req, size_t req_len ) {
	struct net_device *netdev = supplicant->netdev;
	const struct eap_mschapv2 *hdr = req;

	/* Sanity check */
	if ( req_len < sizeof ( *hdr ) ) {
		DBGC ( netdev, "EAP %s underlength MS-CHAPv2:\n",
		       netdev->name );
		DBGC_HDA ( netdev, 0, req, req_len );
		return -EINVAL;
	}

	/* Handle according to opcode */
	switch ( hdr->code ) {
	case EAP_CODE_REQUEST:
		return eap_rx_mschapv2_request ( supplicant, hdr, req_len );
	case EAP_CODE_SUCCESS:
		return eap_rx_mschapv2_success ( supplicant, hdr, req_len );
	default:
		DBGC ( netdev, "EAP %s unsupported MS-CHAPv2 opcode %d\n",
		       netdev->name, hdr->code );
		DBGC_HDA ( netdev, 0, req, req_len );
		return -ENOTSUP;
	}
}

/** EAP MS-CHAPv2 method */
struct eap_method eap_mschapv2_method __eap_method = {
	.type = EAP_TYPE_MSCHAPV2,
	.rx = eap_rx_mschapv2,
};
