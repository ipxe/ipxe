/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/asn1.h>
#include <ipxe/x509.h>

/** @file
 *
 * X.509 certificates
 *
 * The structure of X.509v3 certificates is concisely documented in
 * RFC5280 section 4.1.  The structure of RSA public keys is
 * documented in RFC2313.
 */

/**
 * Identify X.509 certificate RSA public key
 *
 * @v certificate	Certificate
 * @v rsa		RSA public key to fill in
 * @ret rc		Return status code
 */
int x509_rsa_public_key ( const struct asn1_cursor *certificate,
			  struct x509_rsa_public_key *key ) {
	struct asn1_cursor *cursor = &key->raw;
	int rc;

	/* Locate subjectPublicKeyInfo */
	memcpy ( cursor, certificate, sizeof ( *cursor ) );
	rc = ( asn1_enter ( cursor, ASN1_SEQUENCE ), /* Certificate */
	       asn1_enter ( cursor, ASN1_SEQUENCE ), /* tbsCertificate */
	       asn1_skip_if_exists ( cursor, ASN1_EXPLICIT_TAG(0) ),/*version*/
	       asn1_skip ( cursor, ASN1_INTEGER ), /* serialNumber */
	       asn1_skip ( cursor, ASN1_SEQUENCE ), /* signature */
	       asn1_skip ( cursor, ASN1_SEQUENCE ), /* issuer */
	       asn1_skip ( cursor, ASN1_SEQUENCE ), /* validity */
	       asn1_skip ( cursor, ASN1_SEQUENCE ) /* name */ );
	if ( rc != 0 ) {
		DBG ( "Cannot locate subjectPublicKeyInfo in:\n" );
		DBG_HDA ( 0, certificate->data, certificate->len );
		return rc;
	}

	return 0;
}
