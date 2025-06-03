/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/init.h>
#include <ipxe/uuid.h>
#include <ipxe/smbios.h>

/** SMBIOS settings scope */
static const struct settings_scope smbios_settings_scope;

/**
 * Construct SMBIOS raw-data tag
 *
 * @v _type		SMBIOS structure type number
 * @v _structure	SMBIOS structure data type
 * @v _field		Field within SMBIOS structure data type
 * @ret tag		SMBIOS setting tag
 */
#define SMBIOS_RAW_TAG( _type, _structure, _field )		\
	( ( (_type) << 16 ) |					\
	  ( offsetof ( _structure, _field ) << 8 ) |		\
	  ( sizeof ( ( ( _structure * ) 0 )->_field ) ) )

/**
 * Construct SMBIOS string tag
 *
 * @v _type		SMBIOS structure type number
 * @v _structure	SMBIOS structure data type
 * @v _field		Field within SMBIOS structure data type
 * @ret tag		SMBIOS setting tag
 */
#define SMBIOS_STRING_TAG( _type, _structure, _field )		\
	( ( (_type) << 16 ) |					\
	  ( offsetof ( _structure, _field ) << 8 ) )

/**
 * Check applicability of SMBIOS setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int smbios_applies ( struct settings *settings __unused,
			    const struct setting *setting ) {

	return ( setting->scope == &smbios_settings_scope );
}

/**
 * Fetch value of SMBIOS setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int smbios_fetch ( struct settings *settings, struct setting *setting,
			  void *data, size_t len ) {
	const struct smbios_header *structure;
	unsigned int tag_instance;
	unsigned int tag_type;
	unsigned int tag_offset;
	unsigned int tag_len;
	const void *src;
	size_t src_len;
	unsigned int string;
	union uuid uuid;

	/* Split tag into instance, type, offset and length */
	tag_instance = ( ( setting->tag >> 24 ) & 0xff );
	tag_type = ( ( setting->tag >> 16 ) & 0xff );
	tag_offset = ( ( setting->tag >> 8 ) & 0xff );
	tag_len = ( setting->tag & 0xff );

	/* Find SMBIOS structure */
	structure = smbios_structure ( tag_type, tag_instance );
	if ( ! structure )
		return -ENOENT;
	src = structure;
	src_len = structure->len;
	string = 0;

	/* A <length> of zero indicates that the byte at <offset>
	 * contains a string index.  An <offset> of zero indicates
	 * that the <length> contains a literal string index.
	 *
	 * Since the byte at offset zero can never contain a string
	 * index, and a literal string index can never be zero, the
	 * combination of both <length> and <offset> being zero
	 * indicates that the entire structure is to be read.
	 */
	if ( ( tag_len == 0 ) && ( tag_offset == 0 ) ) {
		/* Read whole structure */
	} else if ( ( tag_len == 0 ) || ( tag_offset == 0 ) ) {
		/* Read string */
		string = tag_len;
		if ( ( string == 0 ) && ( tag_offset < src_len ) )
			string = *( ( uint8_t * ) src + tag_offset );
		src = smbios_string ( structure, string );
		if ( ! src )
			return -ENOENT;
		assert ( string > 0 );
		src_len = strlen ( src );
	} else if ( tag_offset > src_len ) {
		/* Empty read beyond end of structure */
		src_len = 0;
	} else {
		/* Read partial structure */
		src += tag_offset;
		src_len -= tag_offset;
		if ( src_len > tag_len )
			src_len = tag_len;
	}

	/* Mangle UUIDs if necessary.  iPXE treats UUIDs as being in
	 * network byte order (big-endian).  SMBIOS specification
	 * version 2.6 states that UUIDs are stored with little-endian
	 * values in the first three fields; earlier versions did not
	 * specify an endianness.  dmidecode assumes that the byte
	 * order is little-endian if and only if the SMBIOS version is
	 * 2.6 or higher; we match this behaviour.
	 */
	if ( ( ( setting->type == &setting_type_uuid ) ||
	       ( setting->type == &setting_type_guid ) ) &&
	     ( src_len == sizeof ( uuid ) ) &&
	     ( smbios_version() >= SMBIOS_VERSION ( 2, 6 ) ) ) {
		DBGC ( settings, "SMBIOS detected mangled UUID\n" );
		memcpy ( &uuid, src, sizeof ( uuid ) );
		uuid_mangle ( &uuid );
		src = &uuid;
	}

	/* Return data */
	if ( len > src_len )
		len = src_len;
	memcpy ( data, src, len );

	/* Set default type */
	if ( ! setting->type ) {
		setting->type = ( string ? &setting_type_string :
				  &setting_type_hex );
	}

	return src_len;
}

/** SMBIOS settings operations */
static struct settings_operations smbios_settings_operations = {
	.applies = smbios_applies,
	.fetch = smbios_fetch,
};

/** SMBIOS settings */
static struct settings smbios_settings = {
	.refcnt = NULL,
	.siblings = LIST_HEAD_INIT ( smbios_settings.siblings ),
	.children = LIST_HEAD_INIT ( smbios_settings.children ),
	.op = &smbios_settings_operations,
	.default_scope = &smbios_settings_scope,
};

/** Initialise SMBIOS settings */
static void smbios_init ( void ) {
	struct settings *settings = &smbios_settings;
	int rc;

	if ( ( rc = register_settings ( settings, NULL, "smbios" ) ) != 0 ) {
		DBGC ( settings, "SMBIOS could not register settings: %s\n",
		       strerror ( rc ) );
		return;
	}
}

/** SMBIOS settings initialiser */
struct init_fn smbios_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = smbios_init,
};

/** UUID setting obtained via SMBIOS */
const struct setting uuid_setting __setting ( SETTING_HOST, uuid ) = {
	.name = "uuid",
	.description = "UUID",
	.tag = SMBIOS_RAW_TAG ( SMBIOS_TYPE_SYSTEM_INFORMATION,
				struct smbios_system_information, uuid ),
	.type = &setting_type_uuid,
	.scope = &smbios_settings_scope,
};

/** Manufacturer name setting */
const struct setting manufacturer_setting __setting ( SETTING_HOST_EXTRA,
						      manufacturer ) = {
	.name = "manufacturer",
	.description = "Manufacturer",
	.tag = SMBIOS_STRING_TAG ( SMBIOS_TYPE_SYSTEM_INFORMATION,
				   struct smbios_system_information,
				   manufacturer ),
	.type = &setting_type_string,
	.scope = &smbios_settings_scope,
};

/** Product name setting */
const struct setting product_setting __setting ( SETTING_HOST_EXTRA, product )={
	.name = "product",
	.description = "Product name",
	.tag = SMBIOS_STRING_TAG ( SMBIOS_TYPE_SYSTEM_INFORMATION,
				   struct smbios_system_information,
				   product ),
	.type = &setting_type_string,
	.scope = &smbios_settings_scope,
};

/** Serial number setting */
const struct setting serial_setting __setting ( SETTING_HOST_EXTRA, serial ) = {
	.name = "serial",
	.description = "Serial number",
	.tag = SMBIOS_STRING_TAG ( SMBIOS_TYPE_SYSTEM_INFORMATION,
				   struct smbios_system_information,
				   serial ),
	.type = &setting_type_string,
	.scope = &smbios_settings_scope,
};

/** Asset tag setting */
const struct setting asset_setting __setting ( SETTING_HOST_EXTRA, asset ) = {
	.name = "asset",
	.description = "Asset tag",
	.tag = SMBIOS_STRING_TAG ( SMBIOS_TYPE_ENCLOSURE_INFORMATION,
				   struct smbios_enclosure_information,
				   asset_tag ),
	.type = &setting_type_string,
	.scope = &smbios_settings_scope,
};

/** Board serial number setting (may differ from chassis serial number) */
const struct setting board_serial_setting __setting ( SETTING_HOST_EXTRA,
						      board-serial ) = {
	.name = "board-serial",
	.description = "Base board serial",
	.tag = SMBIOS_STRING_TAG ( SMBIOS_TYPE_BASE_BOARD_INFORMATION,
				   struct smbios_base_board_information,
				   serial ),
	.type = &setting_type_string,
	.scope = &smbios_settings_scope,
};
