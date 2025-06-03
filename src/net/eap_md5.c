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
#include <ipxe/md5.h>
#include <ipxe/chap.h>
#include <ipxe/eap.h>

/** @file
 *
 * EAP MD5-Challenge authentication method
 *
 */

/**
 * Handle EAP MD5-Challenge
 *
 * @v supplicant	EAP supplicant
 * @v req		Request type data
 * @v req_len		Length of request type data
 * @ret rc		Return status code
 */
static int eap_rx_md5 ( struct eap_supplicant *supplicant,
			const void *req, size_t req_len ) {
	struct net_device *netdev = supplicant->netdev;
	const struct eap_md5 *md5req = req;
	struct {
		uint8_t len;
		uint8_t value[MD5_DIGEST_SIZE];
	} __attribute__ (( packed )) md5rsp;
	struct chap_response chap;
	void *secret;
	int secret_len;
	int rc;

	/* Sanity checks */
	if ( req_len < sizeof ( *md5req ) ) {
		DBGC ( netdev, "EAP %s underlength MD5-Challenge:\n",
		       netdev->name );
		DBGC_HDA ( netdev, 0, req, req_len );
		rc = -EINVAL;
		goto err_sanity;
	}
	if ( ( req_len - sizeof ( *md5req ) ) < md5req->len ) {
		DBGC ( netdev, "EAP %s truncated MD5-Challenge:\n",
		       netdev->name );
		DBGC_HDA ( netdev, 0, req, req_len );
		rc = -EINVAL;
		goto err_sanity;
	}

	/* Construct response */
	if ( ( rc = chap_init ( &chap, &md5_algorithm ) ) != 0 ) {
		DBGC ( netdev, "EAP %s could not initialise CHAP: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_chap;
	}
	chap_set_identifier ( &chap, supplicant->id );
	secret_len = fetch_raw_setting_copy ( netdev_settings ( netdev ),
					      &password_setting, &secret );
	if ( secret_len < 0 ) {
		rc = secret_len;
		DBGC ( netdev, "EAP %s has no secret: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_secret;
	}
	chap_update ( &chap, secret, secret_len );
	chap_update ( &chap, md5req->value, md5req->len );
	chap_respond ( &chap );
	assert ( chap.response_len == sizeof ( md5rsp.value ) );
	md5rsp.len = sizeof ( md5rsp.value );
	memcpy ( md5rsp.value, chap.response, sizeof ( md5rsp.value ) );

	/* Transmit response */
	if ( ( rc = eap_tx_response ( supplicant, &md5rsp,
				      sizeof ( md5rsp ) ) ) != 0 )
		goto err_tx;

 err_tx:
	free ( secret );
 err_secret:
	chap_finish ( &chap );
 err_chap:
 err_sanity:
	return rc;
}

/** EAP MD5-Challenge method */
struct eap_method eap_md5_method __eap_method = {
	.type = EAP_TYPE_MD5,
	.rx = eap_rx_md5,
};
