/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * Classic TLS static RSA pre-master secret
 *
 * With classic static RSA key transport, the client unilaterally
 * constructs the shared pre-master secret and then encrypts it using
 * the server's public key.
 *
 * We model key transport as a key exchange algorithm that is
 * incapable of generating public keys and where the public key size
 * is zero (implying that the shared secret must be communicated via a
 * means other than key exchange).
 *
 * This RSA pre-master secret structure could in principle have been
 * used with any public-key algorithm that supports encryption and
 * decryption (rather than only signing and verification), but no
 * non-RSA cipher suites were ever defined to use this exact same
 * structure of the pre-master secret.
 *
 * Key transport provides no forward secrecy since a compromise of the
 * server's long-term private key provides the ability to decrypt all
 * pre-master secrets that were encrypted using that key.  Almost all
 * servers will prefer to use ephemeral key exhange (which does
 * provide forward secrecy).  We retain support for key transport only
 * for the sake of backwards compatibility with older servers.
 *
 */

#include <string.h>
#include <byteswap.h>
#include <ipxe/tls.h>
#include <ipxe/crypto.h>
#include <config/crypto.h>

/** A classic pre-master private key */
struct tls_classic_pre_master_private {
	/** Random bytes */
	uint8_t random[46];
} __attribute__ (( packed ));

/** A classic pre-master shared secret */
struct tls_classic_pre_master_shared {
	/** Highest supported protocol version */
	uint16_t version;
	/** Private key */
	struct tls_classic_pre_master_private private;
} __attribute__ (( packed ));

/**
 * Agree classic pre-master secret
 *
 * @v exchange		Key exchange algorithm
 * @v private		Private key
 * @v partner		Partner public key
 * @v shared		Shared secret to fill in
 * @ret rc		Return status code
 */
static int
tls_classic_pre_master_agree ( struct exchange_algorithm *exchange __unused,
			       const void *private,
			       const void *partner __unused, void *shared ) {
	struct tls_classic_pre_master_shared *premaster = shared;

	/* We model the classic pre-master secret as a key exchange
	 * algorithm in which we unilaterally construct the shared
	 * secret (with no partner public key input).
	 */
	premaster->version = htons ( TLS_VERSION_MAX );
	memcpy ( &premaster->private, private, sizeof ( premaster->private ) );

	return 0;
}

/** Classic pre-master secret key exchange algorithm */
struct exchange_algorithm tls_classic_pre_master_algorithm = {
	.name = "classic pre-master",
	.privsize = sizeof ( struct tls_classic_pre_master_private ),
	.pubsize = 0,
	.sharedsize = sizeof ( struct tls_classic_pre_master_shared ),
	.share = exchange_null_share,
	.agree = tls_classic_pre_master_agree,
};
