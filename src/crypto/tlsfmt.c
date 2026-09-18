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
 * TLS data formats
 *
 */

#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/tls.h>
#include <ipxe/tlsfmt.h>

/**
 * Get data structure descriptor mapping name (for debugging)
 *
 * @v map		Data structure descriptor mapping
 * @ret name		Data structure name
 */
static const char * tls_map_name ( const uint8_t *map ) {

	if ( map == tls_certificate_map ) {
		return "Certificate";
	} else if ( map == tls_certificate_entry_map ) {
		return "CertificateEntry";
	} else if ( map == tls_digitally_signed_map ) {
		return "DigitallySigned";
	} else if ( map == tls_extension_map ) {
		return "Extension";
	} else if ( map == tls_hello_request_map ) {
		return "HelloRequest";
	} else if ( map == tls_key_share_entry_map ) {
		return "KeyShareEntry";
	} else if ( map == tls_new_session_ticket_map ) {
		return "NewSessionTicket";
	} else if ( map == tls_renegotiation_info_map ) {
		return "RenegotiationInfo";
	} else if ( map == tls_server_hello_map ) {
		return "ServerHello";
	} else if ( map == tls_server_hello_done_map ) {
		return "ServerHelloDone";
	} else if ( ( map == tls_server_key_exchange_dhe_map ) ||
		    ( map == tls_server_key_exchange_ecdhe_map ) ) {
		return "ServerKeyExchange";
	} else if ( ( map == tls_supported_versions_map ) ||
		    ( map == tls_supported_version_map ) ) {
		return "SupportedVersions";
	} else {
		return "<UNKNOWN>";
	}
}

/**
 * Parse TLS extensions within a data structure
 *
 * @v map		Data structure descriptor mapping
 * @v index		Current index within mapping
 * @v fields		Extensions data fields
 * @v count		Number of extensions
 * @ret rc		Return status code
 */
static int tls_parse_extensions ( const uint8_t *map, unsigned int index,
				  struct tls_cursor *fields,
				  unsigned int count ) {
	const uint16_t __attribute__ (( aligned ( 1 ) )) *invtypes;
	const struct tls_cursor *cursor = &fields[0];
	struct tls_extension extension;
	unsigned int i;
	uint16_t type;
	int rc;

	/* Extension types are encoded as inverted */
	invtypes = ( ( const void * ) &map[index] );
	for ( i = 1 ; i <= count ; i++ ) {
		if ( ! invtypes[i] ) {
			DBGC ( map, "TLSFMT %s #%d mapping missing extension "
			       "%d\n", tls_map_name ( map ), index, i );
			return -EINVAL;
		}
	}

	/* Parse extensions */
	for ( ; cursor->len ; cursor = &extension.next ) {

		/* Parse next extension */
		if ( ( rc = tls_parse ( tls_extension, TLS_VERSION_BASE,
					cursor, &extension ) ) != 0 )
			return rc;

		/* Scan for matching extensions */
		for ( i = 1 ; i <= count ; i++ ) {
			type = ~invtypes[i];
			if ( type != *(extension.type) )
				continue;
			if ( fields[i].data ) {
				DBGC ( map, "TLSFMT %s #%d has duplicate "
				       "extension %#04x\n",
				       tls_map_name ( map ), index,
				       ntohs ( type ) );
				return -EPROTO;
			}
			fields[i].data = extension.data.data;
			fields[i].len = extension.data.len;
			DBGC2 ( map, "TLSFMT %s #%d extension %#04x:%s\n",
				tls_map_name ( map ), index, ntohs ( type ),
				( fields[i].len ? "" : " (empty)" ) );
			DBGC2_HDA ( map, 0, fields[i].data, fields[i].len );
		}
	}

	return 0;
}

/**
 * Parse TLS data structure
 *
 * @v map		Data structure descriptor mapping
 * @v version		Protocol version
 * @v cursor		Cursor containing TLS data structure
 * @v desc		Data structure descriptor to fill in
 * @ret rc		Return status code
 */
int tls_parse_map ( const uint8_t *map, unsigned int version,
		    const struct tls_cursor *cursor,
		    union tls_ptr_len *desc ) {
	struct tls_cursor *field;
	const void *data;
	size_t remaining;
	unsigned int count;
	unsigned int index;
	unsigned int next;
	unsigned int byte;
	unsigned int minimum;
	unsigned int len_len;
	unsigned int len;
	int extensions;
	int fixed;
	int rc;

	/* Read initial cursor (may overlap output data structure) */
	data = cursor->data;
	remaining = cursor->len;
	DBGC2 ( map, "TLSFMT %s parsing:%s\n",
		tls_map_name ( map ), ( remaining ? "" : " (empty)" ) );
	DBGC2_HDA ( map, 0, data, remaining );

	/* Get mapping length */
	count = map[0];
	if ( ! count ) {
		DBGC ( map, "TLSFMT %s mapping has no count\n",
		       tls_map_name ( map ) );
		return -EINVAL;
	}

	/* Clear data structure descriptor */
	memset ( desc, 0, ( ( count - 1 ) * sizeof ( desc[0] ) ) );

	/* Parse data via mapping */
	for ( index = 1 ; ( next = index ) < count ; index = next ) {

		/* Prepare to store field description */
		field = container_of ( &desc[ index - 1 ].data,
				       struct tls_cursor, data );

		/* Interpret byte as `llllllvv` */
		byte = map[next++];
		minimum = TLS_MAP_MIN ( byte );
		fixed = TLS_MAP_FIXED ( byte );
		if ( fixed < 0 ) {
			DBGC ( map, "TLSFMT %s #%d mapping missing\n",
			       tls_map_name ( map ), index );
			return -EINVAL;
		}
		assert ( next <= count );

		/* Handle fixed-length fields */
		if ( fixed ) {
			if ( version < minimum ) {
				DBGC2 ( map, "TLSFMT %s #%d not in version "
					"%d.%d\n", tls_map_name ( map ),
					index, ( version >> 8 ),
					( version & 0xff ) );
				continue;
			}
			if ( ( ( size_t ) fixed ) > remaining ) {
				DBGC ( map, "TLSFMT %s #%d too short for "
				       "fixed length %d:\n",
				       tls_map_name ( map ), index, fixed );
				DBGC_HDA ( map, 0, data, remaining );
				return -EPROTO;
			}
			field->data = data;
			DBGC2 ( map, "TLSFMT %s #%d:\n",
				tls_map_name ( map ), index );
			DBGC2_HDA ( map, 0, field->data, fixed );
			data += fixed;
			remaining -= fixed;
			continue;
		}

		/* Interpret next byte as `xxxxxxnn` */
		if ( next >= count ) {
			DBGC ( map, "TLSFMT %s #%d mapping overrun\n",
			       tls_map_name ( map ), index );
			return -EINVAL;
		}
		byte = map[next++];
		len_len = TLS_MAP_LEN_LEN ( byte );
		extensions = TLS_MAP_EXTENSIONS ( byte );
		if ( extensions >= 0 )
			next += ( extensions * 2 );
		if ( next > count ) {
			DBGC ( map, "TLSFMT %s #%d mapping extensions "
			       "overrun\n", tls_map_name ( map ), index );
			return -EINVAL;
		}
		if ( version < minimum ) {
			DBGC2 ( map, "TLSFMT %s #%d not in version %d.%d\n",
				tls_map_name ( map ), index, ( version >> 8 ),
				( version & 0xff ) );
			continue;
		}

		/* Allow for optional extensions fields */
		if ( ( extensions >= 0 ) && ( ! remaining ) &&
		     ( version < TLS_VERSION_TLS_1_3 ) ) {
			DBGC2 ( map, "TLSFMT %s #%d optional in version "
				"%d.%d\n", tls_map_name ( map ), index,
				( version >> 8 ), ( version & 0xff ) );
			continue;
		}

		/* Handle variable-length fields */
		if ( remaining < len_len ) {
			DBGC ( map, "TLSFMT %s #%d too short for %d-byte "
			       "variable length:\n", tls_map_name ( map ),
			       index, len_len );
			DBGC_HDA ( map, 0, data, remaining );
			return -EPROTO;
		}
		if ( len_len ) {
			for ( len = 0 ; len_len ; len_len-- ) {
				len <<= 8;
				len |= *( ( const uint8_t * ) data++ );
				remaining--;
			}
		} else {
			len = remaining;
		}
		if ( remaining < len ) {
			DBGC ( map, "TLSFMT %s #%d too short for variable "
			       "length %d:\n",
			       tls_map_name ( map ), index, len );
			DBGC_HDA ( map, 0, data, remaining );
			return -EPROTO;
		}
		field->data = data;
		field->len = len;
		DBGC2 ( map, "TLSFMT %s #%d:%s\n", tls_map_name ( map ),
			index, ( field->len ? "" : " (empty)" ) );
		DBGC2_HDA ( map, 0, field->data, field->len );
		data += len;
		remaining -= len;
		if ( extensions < 0 )
			continue;

		/* Handle fields containing extensions */
		if ( ( rc = tls_parse_extensions ( map, index, field,
						   extensions ) ) != 0 ) {
			return rc;
		}
	}

	/* Check that all data was consumed */
	if ( remaining ) {
		DBGC ( map, "TLSFMT %s has excess data:\n",
		       tls_map_name ( map ) );
		DBGC_HDA ( map, 0, data, remaining );
		return -EPROTO;
	}

	return 0;
}

/**
 * Parse optional TLS data structure
 *
 * @v map		Data structure descriptor mapping
 * @v version		Protocol version
 * @v cursor		Cursor containing TLS data structure
 * @v desc		Data structure descriptor to fill in
 * @ret rc		Return status code
 */
int tls_parse_opt_map ( const uint8_t *map, unsigned int version,
			const struct tls_cursor *cursor,
			union tls_ptr_len *desc ) {
	unsigned int count;
	int rc;

	/* Get mapping length */
	count = map[0];
	if ( ! count ) {
		DBGC ( map, "TLSFMT %s mapping has no count\n",
		       tls_map_name ( map ) );
		return -EINVAL;
	}

	/* Clear data structure descriptor */
	memset ( desc, 0, ( ( count - 1 ) * sizeof ( desc[0] ) ) );

	/* Do nothing if optional data structure is not present */
	if ( ! cursor->data )
		return 0;

	/* Parse data structure */
	if ( ( rc = tls_parse_map ( map, version, cursor, desc ) ) != 0 )
		return rc;

	return 0;
}

/** Certificate descriptor mapping */
TLS_DESCR_MAPPING ( tls_certificate ) = {
	TLS_MAPSZ ( tls_certificate ),
	TLS_VAR08 ( tls_certificate, TLS_VERSION_TLS_1_3, context ),
	TLS_VAR24 ( tls_certificate, TLS_VERSION_BASE, list ),
};

/** CertificateEntry descriptor mapping */
TLS_DESCR_MAPPING ( tls_certificate_entry ) = {
	TLS_MAPSZ ( tls_certificate_entry ),
	TLS_VAR24 ( tls_certificate_entry, TLS_VERSION_BASE, cert ),
	TLS_EXT16 ( tls_certificate_entry, TLS_VERSION_TLS_1_3, ext ),
	TLS_EXTRA ( tls_certificate_entry, TLS_VERSION_BASE, next ),
};

/** DigitallySigned descriptor mapping */
TLS_DESCR_MAPPING ( tls_digitally_signed ) = {
	TLS_MAPSZ ( tls_digitally_signed ),
	TLS_FIXED ( tls_digitally_signed, TLS_VERSION_TLS_1_2, sig_hash ),
	TLS_VAR16 ( tls_digitally_signed, TLS_VERSION_BASE, sig ),
};

/** Extension descriptor mapping */
TLS_DESCR_MAPPING ( tls_extension ) = {
	TLS_MAPSZ ( tls_extension ),
	TLS_FIXED ( tls_extension, TLS_VERSION_BASE, type ),
	TLS_VAR16 ( tls_extension, TLS_VERSION_BASE, data ),
	TLS_EXTRA ( tls_extension, TLS_VERSION_BASE, next ),
};

/** HelloRequest descriptor mapping */
TLS_DESCR_MAPPING ( tls_hello_request ) = {
	TLS_MAPSZ ( tls_hello_request ),
};

/** KeyShareEntry descriptor mapping */
TLS_DESCR_MAPPING ( tls_key_share_entry ) = {
	TLS_MAPSZ ( tls_key_share_entry ),
	TLS_FIXED ( tls_key_share_entry, TLS_VERSION_BASE, group ),
	TLS_VAR16 ( tls_key_share_entry, TLS_VERSION_BASE, public ),
	TLS_EXTRA ( tls_key_share_entry, TLS_VERSION_BASE, next ),
};

/** NewSessionTicket descriptor mapping */
TLS_DESCR_MAPPING ( tls_new_session_ticket ) = {
	TLS_MAPSZ ( tls_new_session_ticket ),
	TLS_FIXED ( tls_new_session_ticket, TLS_VERSION_BASE, lifetime ),
	TLS_FIXED ( tls_new_session_ticket, TLS_VERSION_TLS_1_3, age ),
	TLS_VAR08 ( tls_new_session_ticket, TLS_VERSION_TLS_1_3, nonce ),
	TLS_VAR16 ( tls_new_session_ticket, TLS_VERSION_BASE, ticket ),
	TLS_EXT16 ( tls_new_session_ticket, TLS_VERSION_TLS_1_3, ext ),
};

/** RenegotiationInfo descriptor mapping */
TLS_DESCR_MAPPING ( tls_renegotiation_info ) = {
	TLS_MAPSZ ( tls_renegotiation_info ),
	TLS_VAR08 ( tls_renegotiation_info, TLS_VERSION_BASE, verify ),
};

/** ServerHello descriptor mapping */
TLS_DESCR_MAPPING ( tls_server_hello ) = {
	TLS_MAPSZ ( tls_server_hello ),
	TLS_FIXED ( tls_server_hello, TLS_VERSION_BASE, a ),
	TLS_VAR08 ( tls_server_hello, TLS_VERSION_BASE, session_id ),
	TLS_FIXED ( tls_server_hello, TLS_VERSION_BASE, b ),
	TLS_EXT16 ( tls_server_hello, TLS_VERSION_BASE, ext ),
	TLS_EXTND ( tls_server_hello, TLS_RENEGOTIATION_INFO, ext.reneg ),
	TLS_EXTND ( tls_server_hello, TLS_EXTENDED_MASTER_SECRET, ext.ems ),
	TLS_EXTND ( tls_server_hello, TLS_SUPPORTED_VERSIONS, ext.supver ),
	TLS_EXTND ( tls_server_hello, TLS_KEY_SHARE, ext.key ),
};

/** ServerHello descriptor mapping */
TLS_DESCR_MAPPING ( tls_server_hello_done ) = {
	TLS_MAPSZ ( tls_server_hello_done ),
};

/** ServerKeyExchange descriptor mapping (for DHE) */
TLS_DESCR_MAPPING ( tls_server_key_exchange_dhe ) = {
	TLS_MAPSZ ( tls_server_key_exchange_dhe ),
	TLS_VAR16 ( tls_server_key_exchange_dhe, TLS_VERSION_BASE, dh_p ),
	TLS_VAR16 ( tls_server_key_exchange_dhe, TLS_VERSION_BASE, dh_g ),
	TLS_VAR16 ( tls_server_key_exchange_dhe, TLS_VERSION_BASE, dh_ys ),
	TLS_EXTRA ( tls_server_key_exchange_dhe, TLS_VERSION_BASE, dsig ),
};

/** ServerKeyExchange descriptor mapping (for EcDHE) */
TLS_DESCR_MAPPING ( tls_server_key_exchange_ecdhe ) = {
	TLS_MAPSZ ( tls_server_key_exchange_ecdhe ),
	TLS_FIXED ( tls_server_key_exchange_ecdhe, TLS_VERSION_BASE, curve ),
	TLS_VAR08 ( tls_server_key_exchange_ecdhe, TLS_VERSION_BASE, point ),
	TLS_EXTRA ( tls_server_key_exchange_ecdhe, TLS_VERSION_BASE, dsig ),
};

/** SupportedVersions descriptor mapping (in ServerHello) */
TLS_DESCR_MAPPING ( tls_supported_version ) = {
	TLS_MAPSZ ( tls_supported_version ),
	TLS_FIXED ( tls_supported_version, TLS_VERSION_BASE, selected ),
};

/** SupportedVersions descriptor mapping (in ClientHello) */
TLS_DESCR_MAPPING ( tls_supported_versions ) = {
	TLS_MAPSZ ( tls_supported_versions ),
	TLS_VAR08 ( tls_supported_versions, TLS_VERSION_BASE, versions ),
};
