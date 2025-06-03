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
 * EFI signature lists
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/asn1.h>
#include <ipxe/der.h>
#include <ipxe/pem.h>
#include <ipxe/image.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Guid/ImageAuthentication.h>
#include <ipxe/efi/efi_siglist.h>

/**
 * Find EFI signature list entry
 *
 * @v data		EFI signature list
 * @v len		Length of EFI signature list
 * @v start		Starting offset to update
 * @v lhdr		Signature list header to fill in
 * @v dhdr		Signature data header to fill in
 * @ret rc		Return status code
 */
static int efisig_find ( const void *data, size_t len, size_t *start,
			 const EFI_SIGNATURE_LIST **lhdr,
			 const EFI_SIGNATURE_DATA **dhdr ) {
	size_t offset;
	size_t remaining;
	size_t skip;
	size_t dlen;

	/* Scan through signature list */
	offset = 0;
	while ( 1 ) {

		/* Read list header */
		assert ( offset <= len );
		remaining = ( len - offset );
		if ( remaining < sizeof ( **lhdr ) ) {
			DBGC ( data, "EFISIG [%#zx,%#zx) truncated header "
			       "at +%#zx\n", *start, len, offset );
			return -EINVAL;
		}
		*lhdr = ( data + offset );

		/* Get length of this signature list */
		if ( remaining < (*lhdr)->SignatureListSize ) {
			DBGC ( data, "EFISIG [%#zx,%#zx) truncated list at "
			       "+%#zx\n", *start, len, offset );
			return -EINVAL;
		}
		remaining = (*lhdr)->SignatureListSize;

		/* Get length of each signature in list */
		dlen = (*lhdr)->SignatureSize;
		if ( dlen < sizeof ( **dhdr ) ) {
			DBGC ( data, "EFISIG [%#zx,%#zx) underlength "
			       "signatures at +%#zx\n", *start, len, offset );
			return -EINVAL;
		}

		/* Strip list header (including variable portion) */
		if ( ( remaining < sizeof ( **lhdr ) ) ||
		     ( ( remaining - sizeof ( **lhdr ) ) <
		       (*lhdr)->SignatureHeaderSize ) ) {
			DBGC ( data, "EFISIG [%#zx,%#zx) malformed header at "
			       "+%#zx\n", *start, len, offset );
			return -EINVAL;
		}
		skip = ( sizeof ( **lhdr ) + (*lhdr)->SignatureHeaderSize );
		offset += skip;
		remaining -= skip;

		/* Read signatures */
		for ( ; remaining ; offset += dlen, remaining -= dlen ) {

			/* Check length */
			if ( remaining < dlen ) {
				DBGC ( data, "EFISIG [%#zx,%#zx) truncated "
				       "at +%#zx\n", *start, len, offset );
				return -EINVAL;
			}

			/* Continue until we find the requested signature */
			if ( offset < *start )
				continue;

			/* Read data header */
			*dhdr = ( data + offset );
			DBGC2 ( data, "EFISIG [%#zx,%#zx) %s ",
				offset, ( offset + dlen ),
				efi_guid_ntoa ( &(*lhdr)->SignatureType ) );
			DBGC2 ( data, "owner %s\n",
				efi_guid_ntoa ( &(*dhdr)->SignatureOwner ) );
			*start = offset;
			return 0;
		}
	}
}

/**
 * Extract ASN.1 object from EFI signature list
 *
 * @v data		EFI signature list
 * @v len		Length of EFI signature list
 * @v offset		Offset within image
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next image, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
int efisig_asn1 ( const void *data, size_t len, size_t offset,
		  struct asn1_cursor **cursor ) {
	const EFI_SIGNATURE_LIST *lhdr;
	const EFI_SIGNATURE_DATA *dhdr;
	int ( * asn1 ) ( const void *data, size_t len, size_t offset,
			 struct asn1_cursor **cursor );
	size_t skip = offsetof ( typeof ( *dhdr ), SignatureData );
	int next;
	int rc;

	/* Locate signature list entry */
	if ( ( rc = efisig_find ( data, len, &offset, &lhdr, &dhdr ) ) != 0 )
		goto err_entry;
	len = ( offset + lhdr->SignatureSize );

	/* Parse as PEM or DER based on first character */
	asn1 = ( ( dhdr->SignatureData[0] == ASN1_SEQUENCE ) ?
		 der_asn1 : pem_asn1 );
	DBGC2 ( data, "EFISIG [%#zx,%#zx) extracting %s\n", offset, len,
		( ( asn1 == der_asn1 ) ? "DER" : "PEM" ) );
	next = asn1 ( data, len, ( offset + skip ), cursor );
	if ( next < 0 ) {
		rc = next;
		DBGC ( data, "EFISIG [%#zx,%#zx) could not extract ASN.1: "
		       "%s\n", offset, len, strerror ( rc ) );
		goto err_asn1;
	}

	/* Check that whole entry was consumed */
	if ( ( ( unsigned int ) next ) != len ) {
		DBGC ( data, "EFISIG [%#zx,%#zx) malformed data\n",
		       offset, len );
		rc = -EINVAL;
		goto err_whole;
	}

	return len;

 err_whole:
	free ( *cursor );
 err_asn1:
 err_entry:
	return rc;
}

/**
 * Probe EFI signature list image
 *
 * @v image		EFI signature list
 * @ret rc		Return status code
 */
static int efisig_image_probe ( struct image *image ) {
	const EFI_SIGNATURE_LIST *lhdr;
	const EFI_SIGNATURE_DATA *dhdr;
	size_t offset = 0;
	unsigned int count = 0;
	int rc;

	/* Check file is a well-formed signature list */
	while ( 1 ) {

		/* Find next signature list entry */
		if ( ( rc = efisig_find ( image->data, image->len, &offset,
					  &lhdr, &dhdr ) ) != 0 ) {
			return rc;
		}

		/* Skip this entry */
		offset += lhdr->SignatureSize;
		count++;

		/* Check if we have reached end of the image */
		if ( offset == image->len ) {
			DBGC ( image, "EFISIG %s contains %d signatures\n",
			       image->name, count );
			return 0;
		}
	}
}

/**
 * Extract ASN.1 object from EFI signature list image
 *
 * @v image		EFI signature list
 * @v offset		Offset within image
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next image, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
static int efisig_image_asn1 ( struct image *image, size_t offset,
			       struct asn1_cursor **cursor ) {
	int next;
	int rc;

	/* Extract ASN.1 object */
	if ( ( next = efisig_asn1 ( image->data, image->len, offset,
				    cursor ) ) < 0 ) {
		rc = next;
		DBGC ( image, "EFISIG %s could not extract ASN.1: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return next;
}

/** EFI signature list image type */
struct image_type efisig_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "EFISIG",
	.probe = efisig_image_probe,
	.asn1 = efisig_image_asn1,
};
