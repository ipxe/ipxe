/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * EFI CA certificates
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/x509.h>
#include <ipxe/rootcert.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_siglist.h>
#include <ipxe/efi/Guid/TlsAuthentication.h>

/** List of EFI CA certificates */
static struct x509_chain efi_cacerts = {
	.refcnt = REF_INIT ( ref_no_free ),
	.links = LIST_HEAD_INIT ( efi_cacerts.links ),
};

/**
 * Retrieve EFI CA certificate
 *
 * @v data		TlsCaCertificate variable data
 * @v len		Length of TlsCaCertificate
 * @v offset		Offset within data
 * @v next		Next offset, or negative error
 */
static int efi_cacert ( const void *data, size_t len, size_t offset ) {
	struct asn1_cursor *cursor;
	struct x509_certificate *cert;
	int next;
	int rc;

	/* Extract ASN.1 object */
	next = efisig_asn1 ( data, len, offset, &cursor );
	if ( next < 0 ) {
		rc = next;
		DBGC ( &efi_cacerts, "EFICA could not parse at +%#zx: %s\n",
		       offset, strerror ( rc ) );
		goto err_asn1;
	}

	/* Append to list of EFI CA certificates */
	if ( ( rc = x509_append_raw ( &efi_cacerts, cursor->data,
				      cursor->len ) ) != 0 ) {
		DBGC ( &efi_cacerts, "EFICA could not append at +%#zx: %s\n",
		       offset, strerror ( rc ) );
		goto err_append;
	}
	cert = x509_last ( &efi_cacerts );
	DBGC ( &efi_cacerts, "EFICA found certificate %s\n",
	       x509_name ( cert ) );

	/* Mark certificate as valid (i.e. trusted) if permitted */
	if ( allow_trust_override ) {
		DBGC ( &efi_cacerts, "EFICA trusting certificate %s\n",
		       x509_name ( cert ) );
		x509_set_valid ( cert, NULL, &root_certificates );
	}

	/* Free ASN.1 object */
	free ( cursor );

	return next;

 err_append:
	free ( cursor );
 err_asn1:
	return rc;
}

/**
 * Retrieve all EFI CA certificates
 *
 * @ret rc		Return status code
 */
static int efi_cacert_all ( void ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	EFI_GUID *guid = &efi_tls_ca_certificate_guid;
	static CHAR16 *wname = EFI_TLS_CA_CERTIFICATE_VARIABLE;
	int offset = 0;
	UINT32 attrs;
	UINTN size;
	void *data;
	EFI_STATUS efirc;
	int rc;

	/* Get variable length */
	size = 0;
	if ( ( efirc = rs->GetVariable ( wname, guid, &attrs, &size,
					 NULL ) ) != EFI_BUFFER_TOO_SMALL ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_cacerts, "EFICA could not get %ls size: %s\n",
		       wname, strerror ( rc ) );
		goto err_len;
	}

	/* Allocate temporary buffer */
	data = malloc ( size );
	if ( ! data ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Read variable */
	if ( ( efirc = rs->GetVariable ( wname, guid, &attrs, &size,
					 data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_cacerts, "EFICA could not read %ls: %s\n",
		       wname, strerror ( rc ) );
		goto err_get;
	}

	/* Parse certificates */
	while ( ( ( size_t ) offset ) < size ) {
		offset = efi_cacert ( data, size, offset );
		if ( offset < 0 ) {
			rc = offset;
			goto err_cacert;
		}
	}

	/* Success */
	rc = 0;

 err_cacert:
 err_get:
	free ( data );
 err_alloc:
 err_len:
	return rc;
}

/**
 * Initialise EFI CA certificates
 *
 */
static void efi_cacert_init ( void ) {
	int rc;

	/* Initialise all certificates */
	if ( ( rc = efi_cacert_all() ) != 0 ) {
		DBGC ( &efi_cacert, "EFICA could not initialise: %s\n",
		       strerror ( rc ) );
		/* Nothing we can do at this point */
		return;
	}
}

/** EFI CA certificates initialisation function */
struct init_fn efi_cacert_init_fn __init_fn ( INIT_LATE ) = {
	.initialise = efi_cacert_init,
};

/**
 * Discard any EFI CA certificates
 *
 */
static void efi_cacert_shutdown ( int booting __unused ) {

	/* Drop our references to the certificates */
	DBGC ( &efi_cacert, "EFICA discarding certificates\n" );
	x509_truncate ( &efi_cacerts, NULL );
	assert ( list_empty ( &efi_cacerts.links ) );
}

/** EFI CA certificates shutdown function */
struct startup_fn efi_cacert_shutdown_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "efi_cacert",
	.shutdown = efi_cacert_shutdown,
};
