/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/base64.h>
#include <ipxe/mime.h>

/** @file
 *
 * MIME image format
 *
 * The MIME format is defined in RFC 2045 and RFC 2046 (with reference
 * to RFC 822).  We treat it firstly as a simple single-member archive
 * format where the content is extracted using the specified encoding.
 *
 * We additionally support multipart MIME as an archive format from
 * which we attempt to extract the first body part that has the MIME
 * type "text/x-ipxe".  This makes it possible to store both
 * cloud-init configuration and an iPXE boot script in the same user
 * metadata blob for a cloud instance.
 *
 * Due to its historical origins, MIME is an irritatingly flexible
 * format.  We do not attempt to support all possible MIME files: only
 * those that we might reasonably expect to encounter as cloud
 * instance metadata.
 */

static const struct mime_type * mime_type ( const char *name );

/**
 * Parse MIME attribute
 *
 * @v image		MIME image
 * @v value		Header value
 * @v eol		End of header line
 * @v name		Attribute name (including terminating equals sign)
 * @v attr		MIME attribute to update
 */
static void mime_parse_attribute ( struct image *image, const char *value,
				   const char *eol, const char *name,
				   struct mime_attribute *attr ) {
	const char *match;

	/* Locate attribute */
	match = strcasestr ( value, name );
	if ( ( ! match ) || ( match >= eol ) )
		return;
	attr->value = ( match + strlen ( name ) );
	DBGC2 ( image, "MIME %s found %s\"", image->name, name );

	/* Determine attribute length */
	if ( attr->value[0] == '"' )
		attr->value++;
	attr->len = 0;
	while ( attr->value[attr->len] && ( attr->value[attr->len] != '"' ) &&
		( ! isspace ( attr->value[attr->len] ) ) ) {
		DBGC2 ( image, "%c", attr->value[attr->len] );
		attr->len++;
	}
	DBGC2 ( image, "\"\n" );
	assert ( &attr->value[attr->len] <= eol );
}

/**
 * Parse Content-Type header
 *
 * @v image		MIME image
 * @v value		Header value
 * @v eol		End of header line
 * @v headers		MIME headers to update
 */
static void mime_parse_type ( struct image *image, const char *value,
			      const char *eol, struct mime_headers *headers ) {

	/* Record content type */
	headers->type = value;
	DBGC2 ( image, "MIME %s found content type\n", image->name );

	/* Check for boundary separator */
	mime_parse_attribute ( image, value, eol, "boundary=",
			       &headers->boundary );
}

/**
 * Parse Content-Transfer-Encoding header
 *
 * @v image		MIME image
 * @v value		Header value
 * @v eol		End of header line
 * @v headers		MIME headers to update
 */
static void mime_parse_encoding ( struct image *image, const char *value,
				  const char *eol __unused,
				  struct mime_headers *headers ) {

	/* Record content transfer encoding */
	headers->encoding = value;
	DBGC2 ( image, "MIME %s found content transfer encoding\n",
		image->name );
}

/** Recognised MIME headers */
const struct mime_header mime_headers[] = {
	{
		.name = "Content-Type:",
		.parse = mime_parse_type,

	},
	{
		.name = "Content-Transfer-Encoding:",
		.parse = mime_parse_encoding,
	},
};

/**
 * Identify MIME header
 *
 * @v line		Header line
 * @ret header		MIME header, or NULL if not recognised
 */
static const struct mime_header * mime_header ( const char *line ) {
	const struct mime_header *header;
	unsigned int i;

	/* Identify MIME header */
	for ( i = 0 ; i < ( sizeof ( mime_headers ) /
			    sizeof ( mime_headers[0] ) ) ; i++ ) {
		header = &mime_headers[i];
		if ( strncasecmp ( line, header->name,
				   strlen ( header->name ) ) == 0 ) {
			return header;
		}
	}

	return NULL;
}

/**
 * Parse MIME headers
 *
 * @v image		MIME image
 * @v text		Start of MIME headers within image
 * @v headers		MIME headers to fill in
 * @ret rc		Return status code
 */
static int mime_parse ( struct image *image, const char *text,
			struct mime_headers *headers ) {
	const struct mime_header *header;
	const char *value;
	const char *line;
	const char *next;
	const char *eol;

	/* Initialise headers */
	memset ( headers, 0, sizeof ( *headers ) );
	headers->encoding = "7bit";

	/* Parse headers until reaching empty-line separator */
	for ( line = text ; ; line = next ) {

		/* Locate end of line */
		eol = strchr ( line, '\n' );
		if ( ! eol ) {
			DBGC ( image, "MIME %s premature end of file\n",
			       image->name );
			return -EINVAL;
		}
		next = ( eol + 1 );

		/* Check for empty line */
		if ( eol == line )
			break;
		if ( eol[-1] == '\r' )
			eol--;
		if ( eol == line )
			break;

		/* Identify header (if recognised) */
		header = mime_header ( line );
		if ( ! header )
			continue;

		/* Locate start of value */
		value = ( line + strlen ( header->name ) );
		while ( ( *value != '\n' ) && isspace ( *value ) )
			value++;

		/* Parse header */
		header->parse ( image, value, eol, headers );
	}

	/* Check for mandatory headers */
	if ( ! headers->type ) {
		DBGC ( image, "MIME %s missing content type\n", image->name );
		return -EINVAL;
	}

	/* Record length of headers */
	headers->len = ( next - text );

	return 0;
}

/**
 * Decode MIME entity with identity encoding
 *
 * @v image		MIME image
 * @v headers		MIME headers
 * @v decoded		Decoded image
 * @ret rc		Return status code
 */
static int mime_decode_identity ( struct image *image,
				  const struct mime_headers *headers,
				  struct image *decoded ) {
	size_t len;
	int rc;

	/* Allocate space */
	len = ( image->len - headers->len );
	if ( ( rc = image_set_len ( decoded, len ) ) != 0 )
		return rc;

	/* Decode data */
	memcpy ( decoded->rwdata, ( image->data + headers->len ), len );

	return 0;
}

/**
 * Decode MIME entity with Base64 encoding
 *
 * @v image		MIME image
 * @v headers		MIME headers
 * @v decoded		Decoded image
 * @ret rc		Return status code
 */
static int mime_decode_base64 ( struct image *image,
				const struct mime_headers *headers,
				struct image *decoded ) {
	const char *encoded;
	size_t max_len;
	int len;
	int rc;

	/* Allocate space for decoded data */
	encoded = ( image->text + headers->len );
	max_len = base64_decoded_max_len ( encoded );
	if ( ( rc = image_set_len ( decoded, max_len ) ) != 0 )
		return rc;

	/* Decode data */
	len = base64_decode ( encoded, decoded->rwdata, decoded->len );
	if ( len < 0 ) {
		rc = len;
		DBGC ( image, "MIME %s could not decode base64: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}
	assert ( ( ( size_t ) len ) <= max_len );

	/* Set decoded length */
	if ( ( rc = image_set_len ( decoded, len ) ) != 0 )
		return rc;

	return 0;
}

/** MIME encodings */
static const struct mime_encoding mime_encodings[] = {
	{
		.name = "7bit",
		.decode = mime_decode_identity,
	},
	{
		.name = "8bit",
		.decode = mime_decode_identity,
	},
	{
		.name = "binary",
		.decode = mime_decode_identity,
	},
	{
		.name = "base64",
		.decode = mime_decode_base64,
	},
};

/**
 * Identify MIME encoding
 *
 * @v name		Encoding
 * @ret encoding	MIME encoding, or NULL if not recognised
 */
static const struct mime_encoding * mime_encoding ( const char *name ) {
	const struct mime_encoding *encoding;
	size_t len;
	unsigned int i;

	/* Identify MIME encoding */
	for ( i = 0 ; i < ( sizeof ( mime_encodings ) /
			    sizeof ( mime_encodings[0] ) ) ; i++ ) {
		encoding = &mime_encodings[i];
		len = strlen ( encoding->name );
		if ( ( strncasecmp ( name, encoding->name, len ) == 0 ) &&
		     ( ( name[len] == ';' ) || isspace ( name[len] ) ) ) {
			return encoding;
		}
	}

	return NULL;
}

/**
 * Extract single MIME entity
 *
 * @v image		MIME image
 * @v headers		MIME headers
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
static int mime_extract_entity ( struct image *image,
				 const struct mime_headers *headers,
				 struct image *extracted ) {
	const struct mime_encoding *encoding;
	int rc;

	/* Identify encoding */
	encoding = mime_encoding ( headers->encoding );
	if ( ! encoding ) {
		DBGC ( image, "MIME %s has unrecognised encoding\n",
		       image->name );
		return -ENOTSUP;
	}
	DBGC ( image, "MIME %s has encoding %s\n",
	       image->name, encoding->name );

	/* Decode via applicable encoding */
	if ( ( rc = encoding->decode ( image, headers, extracted ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Find end of current part in multipart MIME image
 *
 * @v image		MIME image
 * @v boundary		Boundary separator
 * @v pos		Current position within image
 * @ret end		End of current part, or NULL if not found
 */
static const char * mime_part_end ( struct image *image,
				    const struct mime_attribute *boundary,
				    const char *pos ) {

	/* Locate boundary marker */
	while ( 1 ) {
		pos = strstr ( pos, "\n--" );
		if ( ! pos )
			break;
		pos += ( 1 /* newline */ );
		if ( strncmp ( ( pos + 2 /* -- */ ), boundary->value,
			       boundary->len ) != 0 ) {
			continue;
		}
		return pos;
	}

	DBGC ( image, "MIME %s missing boundary marker\n", image->name );
	return NULL;
}

/**
 * Find start of next part in multipart MIME image
 *
 * @v image		MIME image
 * @v boundary		Boundary separator
 * @v pos		End of current part within image
 * @v next		Start of next part, or NULL if not found
 */
static const char * mime_part_next ( struct image *image,
				     const struct mime_attribute *boundary,
				     const char *pos ) {

	/* Skip boundary marker */
	pos += ( 2 /* -- */ + boundary->len );

	/* Check for end boundary */
	if ( strcmp ( pos, "--" ) == 0 )
		return NULL;

	/* Skip trailing whitespace */
	while ( ( *pos != '\n' ) && isspace ( *pos ) )
		pos++;

	/* Check for and skip terminating newline */
	if ( *pos != '\n' ) {
		DBGC ( image, "MIME %s malformed boundary marker\n",
		       image->name );
		return NULL;
	}
	pos++;

	return pos;
}

/**
 * Extract part from multipart MIME image
 *
 * @v image		MIME image
 * @v headers		MIME headers
 * @v extracted		Extracted image
 * @ret rc		Return status code
 *
 * Extraction will produce a MIME image containing only the selected
 * part.  (We rely on the recursive behaviour of image_extract_exec()
 * to extract the content within the extracted part.)
 */
static int mime_extract_part ( struct image *image,
			       const struct mime_headers *headers,
			       struct image *extracted ) {
	const struct mime_attribute *boundary = &headers->boundary;
	const struct mime_type *type;
	struct mime_headers subheaders;
	const char *part;
	const char *end;
	size_t len;
	int rc;

	/* Find end of preamble */
	end = mime_part_end ( image, boundary, image->text );
	if ( ! end )
		return -ENOENT;

	/* Look for a usable part */
	while ( ( part = mime_part_next ( image, boundary, end ) ) &&
		( end = mime_part_end ( image, boundary, part ) ) ) {

		/* Parse part headers */
		DBGC ( image, "MIME %s found part at [%#zx,%#zx)\n",
		       image->name, ( part - image->text ),
		       ( end - image->text ) );
		if ( ( rc = mime_parse ( image, part, &subheaders ) ) != 0 )
			return rc;

		/* Check for a recognised MIME type */
		type = mime_type ( subheaders.type );
		if ( ! type )
			continue;
		DBGC ( image, "MIME %s found part with type %s\n",
		       image->name, type->name );

		/* Extract part */
		len = ( end - part );
		if ( ( rc = image_set_len ( extracted, len ) ) != 0 )
			return rc;
		memcpy ( extracted->rwdata, part, len );
		return 0;
	}

	DBGC ( image, "MIME %s found no usable part\n", image->name );
	return -ENOENT;
}

/**
 * Probe MIME image
 *
 * @v image		Compressed kernel image
 * @ret rc		Return status code
 */
static int mime_probe ( struct image *image ) {
	struct mime_headers headers;
	int rc;

	/* Check for MIME headers */
	if ( ( rc = mime_parse ( image, image->text, &headers ) ) != 0 )
		return rc;

	return 0;
}

/** MIME types */
static const struct mime_type mime_types[] = {
	{
		.name = "multipart/mixed",
		.extract = mime_extract_part,
	},
	{
		.name = "text/x-ipxe",
		.extract = mime_extract_entity,
	},
};

/**
 * Identify MIME type
 *
 * @v name		Encoding
 * @ret type		MIME type, or NULL if not recognised
 */
static const struct mime_type * mime_type ( const char *name ) {
	const struct mime_type *type;
	size_t len;
	unsigned int i;

	/* Identify MIME type */
	for ( i = 0 ; i < ( sizeof ( mime_types ) /
			    sizeof ( mime_types[0] ) ) ; i++ ) {
		type = &mime_types[i];
		len = strlen ( type->name );
		if ( ( strncasecmp ( name, type->name, len ) == 0 ) &&
		     ( ( name[len] == ';' ) || isspace ( name[len] ) ) ) {
			return type;
		}
	}

	return NULL;
}

/**
 * Extract MIME image
 *
 * @v image		Compressed kernel image
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
static int mime_extract ( struct image *image, struct image *extracted ) {
	const struct mime_type *type;
	struct mime_headers headers;
	int rc;

	/* Parse headers */
	if ( ( rc = mime_parse ( image, image->text, &headers ) ) != 0 )
		return rc;

	/* Identify MIME type */
	type = mime_type ( headers.type );
	if ( ! type ) {
		DBGC ( image, "MIME %s has no type\n", image->name );
		return -ENOTSUP;
	}
	DBGC ( image, "MIME %s has type %s\n", image->name, type->name );

	/* Extract image */
	if ( ( rc = type->extract ( image, &headers, extracted ) ) != 0 )
		return rc;

	return 0;
}

/** MIME image type */
struct image_type mime_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "MIME",
	.probe = mime_probe,
	.extract = mime_extract,
	.exec = image_extract_exec,
};
