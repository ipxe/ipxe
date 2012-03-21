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

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/asn1.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/rsa.h>
#include <ipxe/rootcert.h>
#include <ipxe/x509.h>

/** @file
 *
 * X.509 certificates
 *
 * The structure of X.509v3 certificates is documented in RFC 5280
 * section 4.1.
 */

/* Disambiguate the various error causes */
#define ENOTSUP_ALGORITHM \
	__einfo_error ( EINFO_ENOTSUP_ALGORITHM )
#define EINFO_ENOTSUP_ALGORITHM \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x01, "Unsupported algorithm" )
#define ENOTSUP_EXTENSION \
	__einfo_error ( EINFO_ENOTSUP_EXTENSION )
#define EINFO_ENOTSUP_EXTENSION \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x02, "Unsupported extension" )
#define EINVAL_ALGORITHM \
	__einfo_error ( EINFO_EINVAL_ALGORITHM )
#define EINFO_EINVAL_ALGORITHM \
	__einfo_uniqify ( EINFO_EINVAL, 0x01, "Invalid algorithm type" )
#define EINVAL_BIT_STRING \
	__einfo_error ( EINFO_EINVAL_BIT_STRING )
#define EINFO_EINVAL_BIT_STRING \
	__einfo_uniqify ( EINFO_EINVAL, 0x02, "Invalid bit string" )
#define EINVAL_TIME \
	__einfo_error ( EINFO_EINVAL_TIME )
#define EINFO_EINVAL_TIME \
	__einfo_uniqify ( EINFO_EINVAL, 0x03, "Invalid time" )
#define EINVAL_ALGORITHM_MISMATCH \
	__einfo_error ( EINFO_EINVAL_ALGORITHM_MISMATCH )
#define EINFO_EINVAL_ALGORITHM_MISMATCH \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "Signature algorithm mismatch" )
#define EINVAL_PATH_LEN \
	__einfo_error ( EINFO_EINVAL_PATH_LEN )
#define EINFO_EINVAL_PATH_LEN \
	__einfo_uniqify ( EINFO_EINVAL, 0x05, "Invalid pathLenConstraint" )
#define EINVAL_VERSION \
	__einfo_error ( EINFO_EINVAL_VERSION )
#define EINFO_EINVAL_VERSION \
	__einfo_uniqify ( EINFO_EINVAL, 0x06, "Invalid version" )
#define EACCES_WRONG_ISSUER \
	__einfo_error ( EINFO_EACCES_WRONG_ISSUER )
#define EINFO_EACCES_WRONG_ISSUER \
	__einfo_uniqify ( EINFO_EACCES, 0x01, "Wrong issuer" )
#define EACCES_NOT_CA \
	__einfo_error ( EINFO_EACCES_NOT_CA )
#define EINFO_EACCES_NOT_CA \
	__einfo_uniqify ( EINFO_EACCES, 0x02, "Not a CA certificate" )
#define EACCES_KEY_USAGE \
	__einfo_error ( EINFO_EACCES_KEY_USAGE )
#define EINFO_EACCES_KEY_USAGE \
	__einfo_uniqify ( EINFO_EACCES, 0x03, "Incorrect key usage" )
#define EACCES_EXPIRED \
	__einfo_error ( EINFO_EACCES_EXPIRED )
#define EINFO_EACCES_EXPIRED \
	__einfo_uniqify ( EINFO_EACCES, 0x04, "Expired (or not yet valid)" )
#define EACCES_PATH_LEN \
	__einfo_error ( EINFO_EACCES_PATH_LEN )
#define EINFO_EACCES_PATH_LEN \
	__einfo_uniqify ( EINFO_EACCES, 0x05, "Maximum path length exceeded" )

/** "commonName" object identifier */
static uint8_t oid_common_name[] = { ASN1_OID_COMMON_NAME };

/** "commonName" object identifier cursor */
static struct asn1_cursor oid_common_name_cursor =
	ASN1_OID_CURSOR ( oid_common_name );

/**
 * Parse X.509 certificate algorithm
 *
 * @v cert		X.509 certificate
 * @v algorithm		Algorithm to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
int x509_parse_pubkey_algorithm ( struct x509_certificate *cert,
				  struct asn1_algorithm **algorithm,
				  const struct asn1_cursor *raw ) {

	/* Parse algorithm */
	*algorithm = asn1_algorithm ( raw );
	if ( ! (*algorithm) ) {
		DBGC ( cert, "X509 %p unrecognised algorithm:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -ENOTSUP_ALGORITHM;
	}

	/* Check algorithm has a public key */
	if ( ! (*algorithm)->pubkey ) {
		DBGC ( cert, "X509 %p algorithm %s is not a public-key "
		       "algorithm:\n", cert, (*algorithm)->name );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_ALGORITHM;
	}

	return 0;
}

/**
 * Parse X.509 certificate signature algorithm
 *
 * @v cert		X.509 certificate
 * @v algorithm		Algorithm to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_signature_algorithm ( struct x509_certificate *cert,
					    struct asn1_algorithm **algorithm,
					    const struct asn1_cursor *raw ) {
	int rc;

	/* Parse algorithm */
	if ( ( rc = x509_parse_pubkey_algorithm ( cert, algorithm,
						  raw ) ) != 0 )
		return rc;

	/* Check algorithm is a signature algorithm */
	if ( ! (*algorithm)->digest ) {
		DBGC ( cert, "X509 %p algorithm %s is not a signature "
		       "algorithm:\n", cert, (*algorithm)->name );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_ALGORITHM;
	}

	return 0;
}

/**
 * Parse X.509 certificate bit string
 *
 * @v cert		X.509 certificate
 * @v bits		Bit string to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_bit_string ( struct x509_certificate *cert,
				   struct x509_bit_string *bits,
				   const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	const struct asn1_bit_string *bit_string;
	size_t len;
	unsigned int unused;
	uint8_t unused_mask;
	const uint8_t *last;
	int rc;

	/* Enter bit string */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	if ( ( rc = asn1_enter ( &cursor, ASN1_BIT_STRING ) ) != 0 ) {
		DBGC ( cert, "X509 %p cannot locate bit string:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return rc;
	}

	/* Validity checks */
	if ( cursor.len < sizeof ( *bit_string ) ) {
		DBGC ( cert, "X509 %p invalid bit string:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_BIT_STRING;
	}
	bit_string = cursor.data;
	len = ( cursor.len - offsetof ( typeof ( *bit_string ), data ) );
	unused = bit_string->unused;
	unused_mask = ( 0xff >> ( 8 - unused ) );
	last = ( bit_string->data + len - 1 );
	if ( ( unused >= 8 ) ||
	     ( ( unused > 0 ) && ( len == 0 ) ) ||
	     ( ( *last & unused_mask ) != 0 ) ) {
		DBGC ( cert, "X509 %p invalid bit string:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_BIT_STRING;
	}

	/* Populate bit string */
	bits->data = &bit_string->data;
	bits->len = len;
	bits->unused = unused;

	return 0;
}

/**
 * Parse X.509 certificate bit string that must be an integral number of bytes
 *
 * @v cert		X.509 certificate
 * @v bits		Bit string to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_integral_bit_string ( struct x509_certificate *cert,
					    struct x509_bit_string *bits,
					    const struct asn1_cursor *raw ) {
	int rc;

	/* Parse bit string */
	if ( ( rc = x509_parse_bit_string ( cert, bits, raw ) ) != 0 )
		return rc;

	/* Check that there are no unused bits at end of string */
	if ( bits->unused ) {
		DBGC ( cert, "X509 %p invalid integral bit string:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_BIT_STRING;
	}

	return 0;
}

/**
 * Parse X.509 certificate time
 *
 * @v cert		X.509 certificate
 * @v time		Time to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 *
 * RFC 5280 section 4.1.2.5 places several restrictions on the allowed
 * formats for UTCTime and GeneralizedTime, and mandates the
 * interpretation of centuryless year values.
 */
static int x509_parse_time ( struct x509_certificate *cert,
			     struct x509_time *time,
			     const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	unsigned int have_century;
	unsigned int type;
	union {
		struct {
			uint8_t century;
			uint8_t year;
			uint8_t month;
			uint8_t day;
			uint8_t hour;
			uint8_t minute;
			uint8_t second;
		} __attribute__ (( packed )) named;
		uint8_t raw[7];
	} pairs;
	struct tm tm;
	const uint8_t *data;
	size_t remaining;
	unsigned int tens;
	unsigned int units;
	unsigned int i;
	int rc;

	/* Determine time format utcTime/generalizedTime */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	type = asn1_type ( &cursor );
	switch ( type ) {
	case ASN1_UTC_TIME:
		have_century = 0;
		break;
	case ASN1_GENERALIZED_TIME:
		have_century = 1;
		break;
	default:
		DBGC ( cert, "X509 %p invalid time type %02x\n", cert, type );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_TIME;
	}

	/* Enter utcTime/generalizedTime */
	if ( ( rc = asn1_enter ( &cursor, type ) ) != 0 ) {
		DBGC ( cert, "X509 %p cannot locate %s time:\n", cert,
		       ( ( type == ASN1_UTC_TIME ) ? "UTC" : "generalized" ) );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return rc;
	}

	/* Parse digit string a pair at a time */
	data = cursor.data;
	remaining = cursor.len;
	for ( i = ( have_century ? 0 : 1 ) ; i < sizeof ( pairs.raw ) ; i++ ) {
		if ( remaining < 2 ) {
			DBGC ( cert, "X509 %p invalid time:\n", cert );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return -EINVAL_TIME;
		}
		tens = data[0];
		units = data[1];
		if ( ! ( isdigit ( tens ) && isdigit ( units ) ) ) {
			DBGC ( cert, "X509 %p invalid time:\n", cert );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return -EINVAL_TIME;
		}
		pairs.raw[i] = ( ( 10 * ( tens - '0' ) ) + ( units - '0' ) );
		data += 2;
		remaining -= 2;
	}

	/* Determine century if applicable */
	if ( ! have_century )
		pairs.named.century = ( ( pairs.named.year >= 50 ) ? 19 : 20 );

	/* Check for trailing "Z" */
	if ( ( remaining != 1 ) || ( data[0] != 'Z' ) ) {
		DBGC ( cert, "X509 %p invalid time:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_TIME;
	}

	/* Fill in time */
	tm.tm_year = ( ( ( pairs.named.century - 19 ) * 100 ) +
		       pairs.named.year );
	tm.tm_mon = ( pairs.named.month - 1 );
	tm.tm_mday = pairs.named.day;
	tm.tm_hour = pairs.named.hour;
	tm.tm_min = pairs.named.minute;
	tm.tm_sec = pairs.named.second;

	/* Convert to seconds since the Epoch */
	time->time = mktime ( &tm );

	return 0;
}

/**
 * Parse X.509 certificate version
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_version ( struct x509_certificate *cert,
				const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int version;
	int rc;

	/* Enter version */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Parse integer */
	if ( ( rc = asn1_integer ( &cursor, &version ) ) != 0 ) {
		DBGC ( cert, "X509 %p cannot parse version: %s\n",
		       cert, strerror ( rc ) );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return rc;
	}

	/* Sanity check */
	if ( version < 0 ) {
		DBGC ( cert, "X509 %p invalid version %d\n", cert, version );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return -EINVAL_VERSION;
	}

	/* Record version */
	cert->version = version;
	DBGC ( cert, "X509 %p is a version %d certificate\n",
	       cert, ( cert->version + 1 ) );

	return 0;
}

/**
 * Parse X.509 certificate issuer
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_issuer ( struct x509_certificate *cert,
			       const struct asn1_cursor *raw ) {
	struct x509_issuer *issuer = &cert->issuer;
	int rc;

	/* Record raw issuer */
	memcpy ( &issuer->raw, raw, sizeof ( issuer->raw ) );
	if ( ( rc = asn1_shrink ( &issuer->raw, ASN1_SEQUENCE ) ) != 0 ) {
		DBGC ( cert, "X509 %p cannot shrink issuer: %s\n",
		       cert, strerror ( rc ) );
		return rc;
	}
	DBGC ( cert, "X509 %p issuer is:\n", cert );
	DBGC_HDA ( cert, 0, issuer->raw.data, issuer->raw.len );

	return 0;
}

/**
 * Parse X.509 certificate validity
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_validity ( struct x509_certificate *cert,
				 const struct asn1_cursor *raw ) {
	struct x509_validity *validity = &cert->validity;
	struct x509_time *not_before = &validity->not_before;
	struct x509_time *not_after = &validity->not_after;
	struct asn1_cursor cursor;
	int rc;

	/* Enter validity */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse notBefore */
	if ( ( rc = x509_parse_time ( cert, not_before, &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p valid from time %lld\n", cert, not_before->time );
	asn1_skip_any ( &cursor );

	/* Parse notAfter */
	if ( ( rc = x509_parse_time ( cert, not_after, &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p valid until time %lld\n", cert, not_after->time );

	return 0;
}

/**
 * Parse X.509 certificate common name
 *
 * @v cert		X.509 certificate
 * @v name		Common name to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_common_name ( struct x509_certificate *cert,
				    struct x509_name *name,
				    const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct asn1_cursor oid_cursor;
	struct asn1_cursor name_cursor;
	int rc;

	/* Enter name */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Scan through name list */
	for ( ; cursor.len ; asn1_skip_any ( &cursor ) ) {
		memcpy ( &oid_cursor, &cursor, sizeof ( oid_cursor ) );
		asn1_enter ( &oid_cursor, ASN1_SET );
		asn1_enter ( &oid_cursor, ASN1_SEQUENCE );
		memcpy ( &name_cursor, &oid_cursor, sizeof ( name_cursor ) );
		asn1_enter ( &oid_cursor, ASN1_OID );
		if ( asn1_compare ( &oid_common_name_cursor, &oid_cursor ) != 0)
			continue;
		asn1_skip_any ( &name_cursor );
		if ( ( rc = asn1_enter_any ( &name_cursor ) ) != 0 ) {
			DBGC ( cert, "X509 %p cannot locate name:\n", cert );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return rc;
		}
		name->data = name_cursor.data;
		name->len = name_cursor.len;
		return 0;
	}

	DBGC ( cert, "X509 %p no commonName found:\n", cert );
	DBGC_HDA ( cert, 0, raw->data, raw->len );
	return -ENOENT;
}

/**
 * Parse X.509 certificate subject
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_subject ( struct x509_certificate *cert,
				const struct asn1_cursor *raw ) {
	struct x509_subject *subject = &cert->subject;
	struct x509_name *name = &subject->name;
	int rc;

	/* Record raw subject */
	memcpy ( &subject->raw, raw, sizeof ( subject->raw ) );
	asn1_shrink_any ( &subject->raw );
	DBGC ( cert, "X509 %p subject is:\n", cert );
	DBGC_HDA ( cert, 0, subject->raw.data, subject->raw.len );

	/* Parse common name */
	if ( ( rc = x509_parse_common_name ( cert, name, raw ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p common name is:\n", cert );
	DBGC_HDA ( cert, 0, name->data, name->len );

	return 0;
}

/**
 * Parse X.509 certificate public key information
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_public_key ( struct x509_certificate *cert,
				   const struct asn1_cursor *raw ) {
	struct x509_public_key *public_key = &cert->subject.public_key;
	struct asn1_algorithm **algorithm = &public_key->algorithm;
	struct asn1_cursor cursor;
	int rc;

	/* Record raw subjectPublicKeyInfo */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_shrink_any ( &cursor );
	memcpy ( &public_key->raw, &cursor, sizeof ( public_key->raw ) );

	/* Enter subjectPublicKeyInfo */
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse algorithm */
	if ( ( rc = x509_parse_pubkey_algorithm ( cert, algorithm,
						  &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p public key algorithm is %s\n",
	       cert, (*algorithm)->name );
	DBGC ( cert, "X509 %p public key is:\n", cert );
	DBGC_HDA ( cert, 0, public_key->raw.data, public_key->raw.len );

	return 0;
}

/**
 * Parse X.509 certificate basic constraints
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_basic_constraints ( struct x509_certificate *cert,
					  const struct asn1_cursor *raw ) {
	struct x509_basic_constraints *basic = &cert->extensions.basic;
	struct asn1_cursor cursor;
	int ca = 0;
	int path_len;
	int rc;

	/* Enter basicConstraints */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse "cA", if present */
	if ( asn1_type ( &cursor ) == ASN1_BOOLEAN ) {
		ca = asn1_boolean ( &cursor );
		if ( ca < 0 ) {
			rc = ca;
			DBGC ( cert, "X509 %p cannot parse cA: %s\n",
			       cert, strerror ( rc ) );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return rc;
		}
		asn1_skip_any ( &cursor );
	}
	basic->ca = ca;
	DBGC ( cert, "X509 %p is %sa CA certificate\n",
	       cert, ( basic->ca ? "" : "not " ) );

	/* Ignore everything else unless "cA" is true */
	if ( ! ca )
		return 0;

	/* Parse "pathLenConstraint", if present and applicable */
	basic->path_len = -1U; /* Default is unlimited */
	if ( asn1_type ( &cursor ) == ASN1_INTEGER ) {
		if ( ( rc = asn1_integer ( &cursor, &path_len ) ) != 0 ) {
			DBGC ( cert, "X509 %p cannot parse pathLenConstraint: "
			       "%s\n", cert, strerror ( rc ) );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return rc;
		}
		if ( path_len < 0 ) {
			DBGC ( cert, "X509 %p invalid pathLenConstraint %d\n",
			       cert, path_len );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return -EINVAL;
		}
		basic->path_len = path_len;
		DBGC ( cert, "X509 %p path length constraint is %u\n",
		       cert, basic->path_len );
	}

	return 0;
}

/**
 * Parse X.509 certificate key usage
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_key_usage ( struct x509_certificate *cert,
				  const struct asn1_cursor *raw ) {
	struct x509_key_usage *usage = &cert->extensions.usage;
	struct x509_bit_string bit_string;
	const uint8_t *bytes;
	size_t len;
	unsigned int i;
	int rc;

	/* Mark extension as present */
	usage->present = 1;

	/* Parse bit string */
	if ( ( rc = x509_parse_bit_string ( cert, &bit_string, raw ) ) != 0 )
		return rc;

	/* Parse key usage bits */
	bytes = bit_string.data;
	len = bit_string.len;
	if ( len > sizeof ( usage->bits ) )
		len = sizeof ( usage->bits );
	for ( i = 0 ; i < len ; i++ ) {
		usage->bits |= ( *(bytes++) << ( 8 * i ) );
	}
	DBGC ( cert, "X509 %p key usage is %08x\n", cert, usage->bits );

	return 0;
}

/** "id-ce-basicConstraints" object identifier */
static uint8_t oid_ce_basic_constraints[] = { ASN1_OID_BASICCONSTRAINTS };

/** "id-ce-keyUsage" object identifier */
static uint8_t oid_ce_key_usage[] = { ASN1_OID_KEYUSAGE };

/** Supported certificate extensions */
static struct x509_extension x509_extensions[] = {
	{
		.name = "basicConstraints",
		.oid = ASN1_OID_CURSOR ( oid_ce_basic_constraints ),
		.parse = x509_parse_basic_constraints,
	},
	{
		.name = "keyUsage",
		.oid = ASN1_OID_CURSOR ( oid_ce_key_usage ),
		.parse = x509_parse_key_usage,
	},
};

/**
 * Identify X.509 extension by OID
 *
 * @v oid		OID
 * @ret extension	Extension, or NULL
 */
static struct x509_extension *
x509_find_extension ( const struct asn1_cursor *oid ) {
	struct x509_extension *extension;
	unsigned int i;

	for ( i = 0 ; i < ( sizeof ( x509_extensions ) /
			    sizeof ( x509_extensions[0] ) ) ; i++ ) {
		extension = &x509_extensions[i];
		if ( asn1_compare ( &extension->oid, oid ) == 0 )
			return extension;
	}

	return NULL;
}

/**
 * Parse X.509 certificate extension
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_extension ( struct x509_certificate *cert,
				  const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct asn1_cursor subcursor;
	struct x509_extension *extension;
	int is_critical = 0;
	int rc;

	/* Enter extension */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Try to identify extension */
	memcpy ( &subcursor, &cursor, sizeof ( subcursor ) );
	asn1_enter ( &subcursor, ASN1_OID );
	extension = x509_find_extension ( &subcursor );
	asn1_skip_any ( &cursor );
	DBGC ( cert, "X509 %p found extension %s\n",
	       cert, ( extension ? extension->name : "<unknown>" ) );

	/* Identify criticality */
	if ( asn1_type ( &cursor ) == ASN1_BOOLEAN ) {
		is_critical = asn1_boolean ( &cursor );
		if ( is_critical < 0 ) {
			rc = is_critical;
			DBGC ( cert, "X509 %p cannot parse extension "
			       "criticality: %s\n", cert, strerror ( rc ) );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return rc;
		}
		asn1_skip_any ( &cursor );
	}

	/* Handle unknown extensions */
	if ( ! extension ) {
		if ( is_critical ) {
			/* Fail if we cannot handle a critical extension */
			DBGC ( cert, "X509 %p cannot handle critical "
			       "extension:\n", cert );
			DBGC_HDA ( cert, 0, raw->data, raw->len );
			return -ENOTSUP_EXTENSION;
		} else {
			/* Ignore unknown non-critical extensions */
			return 0;
		}
	};

	/* Extract extnValue */
	if ( ( rc = asn1_enter ( &cursor, ASN1_OCTET_STRING ) ) != 0 ) {
		DBGC ( cert, "X509 %p extension missing extnValue:\n", cert );
		DBGC_HDA ( cert, 0, raw->data, raw->len );
		return rc;
	}

	/* Parse extension */
	if ( ( rc = extension->parse ( cert, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse X.509 certificate extensions, if present
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_extensions ( struct x509_certificate *cert,
				   const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter extensions, if present */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 3 ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse each extension in turn */
	while ( cursor.len ) {
		if ( ( rc = x509_parse_extension ( cert, &cursor ) ) != 0 )
			return rc;
		asn1_skip_any ( &cursor );
	}

	return 0;
}

/**
 * Parse X.509 certificate tbsCertificate
 *
 * @v cert		X.509 certificate
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int x509_parse_tbscertificate ( struct x509_certificate *cert,
				       const struct asn1_cursor *raw ) {
	struct asn1_algorithm **algorithm = &cert->signature_algorithm;
	struct asn1_cursor cursor;
	int rc;

	/* Record raw tbsCertificate */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_shrink_any ( &cursor );
	memcpy ( &cert->tbs, &cursor, sizeof ( cert->tbs ) );

	/* Enter tbsCertificate */
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse version, if present */
	if ( asn1_type ( &cursor ) == ASN1_EXPLICIT_TAG ( 0 ) ) {
		if ( ( rc = x509_parse_version ( cert, &cursor ) ) != 0 )
			return rc;
		asn1_skip_any ( &cursor );
	}

	/* Skip serialNumber */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Parse signature */
	if ( ( rc = x509_parse_signature_algorithm ( cert, algorithm,
						     &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p tbsCertificate signature algorithm is %s\n",
	       cert, (*algorithm)->name );
	asn1_skip_any ( &cursor );

	/* Parse issuer */
	if ( ( rc = x509_parse_issuer ( cert, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse validity */
	if ( ( rc = x509_parse_validity ( cert, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse subject */
	if ( ( rc = x509_parse_subject ( cert, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse subjectPublicKeyInfo */
	if ( ( rc = x509_parse_public_key ( cert, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse extensions, if present */
	if ( ( rc = x509_parse_extensions ( cert, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse X.509 certificate from ASN.1 data
 *
 * @v cert		X.509 certificate
 * @v data		Raw certificate data
 * @v len		Length of raw data
 * @ret rc		Return status code
 */
int x509_parse ( struct x509_certificate *cert, const void *data, size_t len ) {
	struct x509_signature *signature = &cert->signature;
	struct asn1_algorithm **signature_algorithm = &signature->algorithm;
	struct x509_bit_string *signature_value = &signature->value;
	struct asn1_cursor cursor;
	int rc;

	/* Initialise certificate */
	memset ( cert, 0, sizeof ( *cert ) );
	cert->raw.data = data;
	cert->raw.len = len;

	/* Enter certificate */
	memcpy ( &cursor, &cert->raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse tbsCertificate */
	if ( ( rc = x509_parse_tbscertificate ( cert, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse signatureAlgorithm */
	if ( ( rc = x509_parse_signature_algorithm ( cert, signature_algorithm,
						     &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p signatureAlgorithm is %s\n",
	       cert, (*signature_algorithm)->name );
	asn1_skip_any ( &cursor );

	/* Parse signatureValue */
	if ( ( rc = x509_parse_integral_bit_string ( cert, signature_value,
						     &cursor ) ) != 0 )
		return rc;
	DBGC ( cert, "X509 %p signatureValue is:\n", cert );
	DBGC_HDA ( cert, 0, signature_value->data, signature_value->len );

	/* Check that algorithm in tbsCertificate matches algorithm in
	 * signature
	 */
	if ( signature->algorithm != (*signature_algorithm) ) {
		DBGC ( cert, "X509 %p signature algorithm %s does not match "
		       "signatureAlgorithm %s\n",
		       cert, signature->algorithm->name,
		       (*signature_algorithm)->name );
		return -EINVAL_ALGORITHM_MISMATCH;
	}

	return 0;
}

/**
 * Verify X.509 certificate signature
 *
 * @v cert		X.509 certificate
 * @v public_key	X.509 public key
 * @ret rc		Return status code
 */
static int x509_check_signature ( struct x509_certificate *cert,
				  struct x509_public_key *public_key ) {
	struct x509_signature *signature = &cert->signature;
	struct asn1_algorithm *algorithm = signature->algorithm;
	struct digest_algorithm *digest = algorithm->digest;
	struct pubkey_algorithm *pubkey = algorithm->pubkey;
	uint8_t digest_ctx[ digest->ctxsize ];
	uint8_t digest_out[ digest->digestsize ];
	uint8_t pubkey_ctx[ pubkey->ctxsize ];
	int rc;

	/* Sanity check */
	assert ( cert->signature_algorithm == cert->signature.algorithm );

	/* Calculate certificate digest */
	digest_init ( digest, digest_ctx );
	digest_update ( digest, digest_ctx, cert->tbs.data, cert->tbs.len );
	digest_final ( digest, digest_ctx, digest_out );
	DBGC ( cert, "X509 %p digest:\n", cert );
	DBGC_HDA ( cert, 0, digest_out, sizeof ( digest_out ) );

	/* Check that signature public key algorithm matches signer */
	if ( public_key->algorithm->pubkey != pubkey ) {
		DBGC ( cert, "X509 %p signature algorithm %s does not match "
		       "signer's algorithm %s\n",
		       cert, algorithm->name, public_key->algorithm->name );
		rc = -EINVAL_ALGORITHM_MISMATCH;
		goto err_mismatch;
	}

	/* Verify signature using signer's public key */
	if ( ( rc = pubkey_init ( pubkey, pubkey_ctx, public_key->raw.data,
				  public_key->raw.len ) ) != 0 ) {
		DBGC ( cert, "X509 %p cannot initialise public key: %s\n",
		       cert, strerror ( rc ) );
		goto err_pubkey_init;
	}
	if ( ( rc = pubkey_verify ( pubkey, pubkey_ctx, digest, digest_out,
				    signature->value.data,
				    signature->value.len ) ) != 0 ) {
		DBGC ( cert, "X509 %p signature verification failed: %s\n",
		       cert, strerror ( rc ) );
		goto err_pubkey_verify;
	}

	/* Success */
	rc = 0;

 err_pubkey_verify:
	pubkey_final ( pubkey, pubkey_ctx );
 err_pubkey_init:
 err_mismatch:
	return rc;
}

/**
 * Validate X.509 certificate against issuer certificate
 *
 * @v cert		X.509 certificate
 * @v issuer		X.509 issuer certificate
 * @ret rc		Return status code
 */
int x509_validate_issuer ( struct x509_certificate *cert,
			   struct x509_certificate *issuer ) {
	struct x509_public_key *public_key = &issuer->subject.public_key;
	int rc;

	/* Check issuer.  In theory, this should be a full X.500 DN
	 * comparison, which would require support for a plethora of
	 * abominations such as TeletexString (which allows the
	 * character set to be changed mid-string using escape codes).
	 * In practice, we assume that anyone who deliberately changes
	 * the encoding of the issuer DN is probably a masochist who
	 * will rather enjoy the process of figuring out exactly why
	 * their certificate doesn't work.
	 *
	 * See http://www.cs.auckland.ac.nz/~pgut001/pubs/x509guide.txt
	 * for some enjoyable ranting on this subject.
	 */
	if ( asn1_compare ( &cert->issuer.raw, &issuer->subject.raw ) != 0 ) {
		DBGC ( cert, "X509 %p issuer does not match X509 %p subject\n",
		       cert, issuer );
		DBGC_HDA ( cert, 0, cert->issuer.raw.data,
			   cert->issuer.raw.len );
		DBGC_HDA ( issuer, 0, issuer->subject.raw.data,
			   issuer->subject.raw.len );
		return -EACCES_WRONG_ISSUER;
	}

	/* Check that issuer is allowed to sign certificates */
	if ( ! issuer->extensions.basic.ca ) {
		DBGC ( issuer, "X509 %p cannot sign X509 %p: not a CA "
		       "certificate\n", issuer, cert );
		return -EACCES_NOT_CA;
	}
	if ( issuer->extensions.usage.present &&
	     ( ! ( issuer->extensions.usage.bits & X509_KEY_CERT_SIGN ) ) ) {
		DBGC ( issuer, "X509 %p cannot sign X509 %p: no keyCertSign "
		       "usage\n", issuer, cert );
		return -EACCES_KEY_USAGE;
	}

	/* Check signature */
	if ( ( rc = x509_check_signature ( cert, public_key ) ) != 0 )
		return rc;

	DBGC ( cert, "X509 %p successfully validated using X509 %p\n",
	       cert, issuer );
	return 0;
}

/**
 * Calculate X.509 certificate fingerprint
 *
 * @v cert		X.509 certificate
 * @v digest		Digest algorithm
 * @v fingerprint	Fingerprint buffer
 */
void x509_fingerprint ( struct x509_certificate *cert,
			struct digest_algorithm *digest, void *fingerprint ) {
	uint8_t ctx[ digest->ctxsize ];

	/* Calculate fingerprint */
	digest_init ( digest, ctx );
	digest_update ( digest, ctx, cert->raw.data, cert->raw.len );
	digest_final ( digest, ctx, fingerprint );
}

/**
 * Validate X.509 root certificate
 *
 * @v cert		X.509 certificate
 * @v root		X.509 root certificate store
 * @ret rc		Return status code
 */
int x509_validate_root ( struct x509_certificate *cert,
			 struct x509_root *root ) {
	struct digest_algorithm *digest = root->digest;
	uint8_t fingerprint[ digest->digestsize ];
	const uint8_t *root_fingerprint = root->fingerprints;
	unsigned int i;

	/* Calculate certificate fingerprint */
	x509_fingerprint ( cert, digest, fingerprint );

	/* Check fingerprint against all root certificates */
	for ( i = 0 ; i < root->count ; i++ ) {
		if ( memcmp ( fingerprint, root_fingerprint,
			      sizeof ( fingerprint ) ) == 0 ) {
			DBGC ( cert, "X509 %p is a root certificate\n", cert );
			return 0;
		}
		root_fingerprint += sizeof ( fingerprint );
	}

	DBGC ( cert, "X509 %p is not a root certificate\n", cert );
	return -ENOENT;
}

/**
 * Validate X.509 certificate validity period
 *
 * @v cert		X.509 certificate
 * @v time		Time at which to validate certificate
 * @ret rc		Return status code
 */
int x509_validate_time ( struct x509_certificate *cert, time_t time ) {
	struct x509_validity *validity = &cert->validity;

	/* Check validity period */
	if ( time < validity->not_before.time ) {
		DBGC ( cert, "X509 %p is not yet valid (at time %lld)\n",
		       cert, time );
		return -EACCES_EXPIRED;
	}
	if ( time > validity->not_after.time ) {
		DBGC ( cert, "X509 %p has expired (at time %lld)\n",
		       cert, time );
		return -EACCES_EXPIRED;
	}

	DBGC ( cert, "X509 %p is valid (at time %lld)\n", cert, time );
	return 0;
}

/**
 * Validate X.509 certificate chain
 *
 * @v parse_next	Parse next X.509 certificate in chain
 * @v context		Context for parse_next()
 * @v time		Time at which to validate certificates
 * @v root		Root certificate store, or NULL to use default
 * @v first		Initial X.509 certificate to fill in, or NULL
 * @ret rc		Return status code
 */
int x509_validate_chain ( int ( * parse_next ) ( struct x509_certificate *cert,
						 void *context ),
			  void *context, time_t time, struct x509_root *root,
			  struct x509_certificate *first ) {
	struct x509_certificate temp[2];
	struct x509_certificate *current = &temp[0];
	struct x509_certificate *next = &temp[1];
	struct x509_certificate *swap;
	unsigned int path_len = 0;
	int rc;

	/* Use default root certificate store if none specified */
	if ( ! root )
		root = &root_certificates;

	/* Get first certificate in chain */
	if ( ( rc = parse_next ( current, context ) ) != 0 ) {
		DBGC ( context, "X509 chain %p could not get first "
		       "certificate: %s\n", context, strerror ( rc ) );
		return rc;
	}

	/* Record first certificate, if applicable */
	if ( first )
		memcpy ( first, current, sizeof ( *first ) );

	/* Process chain */
	while ( 1 ) {

		/* Check that certificate is valid at specified time */
		if ( ( rc = x509_validate_time ( current, time ) ) != 0 )
			return rc;

		/* Succeed if we have reached a root certificate */
		if ( x509_validate_root ( current, root ) == 0 )
			return 0;

		/* Get next certificate in chain */
		if ( ( rc = parse_next ( next, context ) ) != 0 ) {
			DBGC ( context, "X509 chain %p could not get next "
			       "certificate: %s\n", context, strerror ( rc ) );
			return rc;
		}

		/* Validate current certificate against next certificate */
		if ( ( rc = x509_validate_issuer ( current, next ) ) != 0 )
			return rc;

		/* Validate path length constraint */
		if ( path_len > next->extensions.basic.path_len ) {
			DBGC ( context, "X509 chain %p path length %d exceeds "
			       "maximum %d\n", context, path_len,
			       next->extensions.basic.path_len );
			return -EACCES_PATH_LEN;
		}
		path_len++;

		/* Move to next certificate in chain */
		swap = current;
		current = next;
		next = swap;
	}
}
