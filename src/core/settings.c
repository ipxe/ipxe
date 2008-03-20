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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/in.h>
#include <gpxe/vsprintf.h>
#include <gpxe/dhcp.h>
#include <gpxe/settings.h>

/** @file
 *
 * Configuration settings
 *
 */

/** Registered setting types */
static struct setting_type setting_types[0]
	__table_start ( struct setting_type, setting_types );
static struct setting_type setting_types_end[0]
	__table_end ( struct setting_type, setting_types );

/** Registered named settings */
static struct named_setting named_settings[0]
	__table_start ( struct named_setting, named_settings );
static struct named_setting named_settings_end[0]
	__table_end ( struct named_setting, named_settings );

struct setting_type setting_type_hex __setting_type;

/**
 * Obtain printable version of a settings tag number
 *
 * @v tag		Settings tag number
 * @ret name		String representation of the tag
 */
static inline char * setting_tag_name ( unsigned int tag ) {
	static char name[8];

	if ( DHCP_IS_ENCAP_OPT ( tag ) ) {
		snprintf ( name, sizeof ( name ), "%d.%d",
			   DHCP_ENCAPSULATOR ( tag ),
			   DHCP_ENCAPSULATED ( tag ) );
	} else {
		snprintf ( name, sizeof ( name ), "%d", tag );
	}
	return name;
}

/******************************************************************************
 *
 * Registered settings blocks
 *
 ******************************************************************************
 */

/** List of all registered settings */
static struct list_head all_settings = {
	&interactive_settings.list, &interactive_settings.list
};

// Dummy routine just for testing
static int dummy_set ( struct settings *settings, unsigned int tag,
		       const void *data, size_t len ) {
	DBGC ( settings, "Settings %p: set %s to:\n",
	       settings, setting_tag_name ( tag ) );
	DBGC_HD ( settings, data, len );
	return 0;
}

// Dummy routine just for testing
static int dummy_get ( struct settings *settings, unsigned int tag,
		       void *data, size_t len ) {
	unsigned int i;

	DBGC ( settings, "Settings %p: get %s\n",
	       settings, setting_tag_name ( tag ) );
	for ( i = 0 ; i < len ; i++ )
		*( ( ( uint8_t * ) data ) + i ) = i;
	return ( len ? len : 8 );
}

struct settings_operations dummy_settings_operations = {
	.set = dummy_set,
	.get = dummy_get,
};

/** Interactively-edited settings */
struct settings interactive_settings = {
	.refcnt = NULL,
	.name = "",
	.list = { &all_settings, &all_settings },
	.op = &dummy_settings_operations,
};

/**
 * Find named settings block
 *
 * @v name		Name
 * @ret settings	Settings block, or NULL
 */
struct settings * find_settings ( const char *name ) {
	struct settings *settings;

	list_for_each_entry ( settings, &all_settings, list ) {
		if ( strcasecmp ( name, settings->name ) == 0 )
			return settings;
	}
	return NULL;
}

/******************************************************************************
 *
 * Core settings routines
 *
 ******************************************************************************
 */

/**
 * Get value of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 *
 * The actual length of the setting will be returned even if
 * the buffer was too small.
 */
int get_setting ( struct settings *settings, unsigned int tag,
		  void *data, size_t len ) {
	int ret;

	if ( settings ) {
		return settings->op->get ( settings, tag, data, len );
	} else {
		list_for_each_entry ( settings, &all_settings, list ) {
			if ( ( ret = settings->op->get ( settings, tag,
							 data, len ) ) >= 0 )
				return ret;
		}
		return -ENOENT;
	}
}

/**
 * Get length of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @ret len		Length of setting data, or negative error
 *
 * This function can also be used as an existence check for the
 * setting.
 */
int get_setting_len ( struct settings *settings, unsigned int tag ) {
	return get_setting ( settings, tag, NULL, 0 );
}

/**
 * Get value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v data		Buffer to fill with setting string data
 * @v len		Length of buffer
 * @ret len		Length of string setting, or negative error
 *
 * The resulting string is guaranteed to be correctly NUL-terminated.
 * The returned length will be the length of the underlying setting
 * data.
 */
int get_string_setting ( struct settings *settings, unsigned int tag,
			 char *data, size_t len ) {
	memset ( data, 0, len );
	return get_setting ( settings, tag, data, ( len - 1 ) );
}

/**
 * Get value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v inp		IPv4 address to fill in
 * @ret len		Length of setting, or negative error
 */
int get_ipv4_setting ( struct settings *settings, unsigned int tag,
		       struct in_addr *inp ) {
	int len;

	len = get_setting ( settings, tag, inp, sizeof ( *inp ) );
	if ( len < 0 )
		return len;
	if ( len != sizeof ( *inp ) )
		return -ERANGE;
	return len;
}

/**
 * Get value of signed integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int get_int_setting ( struct settings *settings, unsigned int tag,
		      long *value ) {
	union {
		long value;
		uint8_t u8[ sizeof ( long ) ];
		int8_t s8[ sizeof ( long ) ];
	} buf;
	int len;
	int i;

	buf.value = 0;
	len = get_setting ( settings, tag, &buf, sizeof ( buf ) );
	if ( len < 0 )
		return len;
	if ( len > ( int ) sizeof ( buf ) )
		return -ERANGE;

	*value = ( ( buf.s8[0] >= 0 ) ? 0 : -1L );
	for ( i = 0 ; i < len ; i++ ) {
		*value = ( ( *value << 8 ) | buf.u8[i] );
	}

	return len;
}

/**
 * Get value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int get_uint_setting ( struct settings *settings, unsigned int tag,
		       unsigned long *value ) {
	long svalue;
	int len;

	len = get_int_setting ( settings, tag, &svalue );
	if ( len < 0 )
		return len;

	*value = ( svalue & ( -1UL >> ( sizeof ( long ) - len ) ) );

	return len;
}

/******************************************************************************
 *
 * Named and typed setting routines
 *
 ******************************************************************************
 */

/**
 * Set value of typed setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v type		Settings type
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int set_typed_setting ( struct settings *settings,
			unsigned int tag, struct setting_type *type,
			const char *value ) {

	/* NULL value implies deletion.  Avoid imposing the burden of
	 * checking for NULL values on each typed setting's setf()
	 * method.
	 */
	if ( ! value )
		return delete_setting ( settings, tag );
		
	return type->setf ( settings, tag, value );
}

/**
 * Find named setting
 *
 * @v name		Name
 * @ret setting		Named setting, or NULL
 */
static struct named_setting * find_named_setting ( const char *name ) {
	struct named_setting *setting;

	for ( setting = named_settings ; setting < named_settings_end ;
	      setting++ ) {
		if ( strcasecmp ( name, setting->name ) == 0 )
			return setting;
	}
	return NULL;
}

/**
 * Find setting type
 *
 * @v name		Name
 * @ret type		Setting type, or NULL
 */
static struct setting_type * find_setting_type ( const char *name ) {
	struct setting_type *type;

	for ( type = setting_types ; type < setting_types_end ; type++ ) {
		if ( strcasecmp ( name, type->name ) == 0 )
			return type;
	}
	return NULL;
}

/**
 * Parse setting name
 *
 * @v name		Name of setting
 * @ret settings	Settings block, or NULL
 * @ret tag		Setting tag number
 * @ret type		Setting type
 * @ret rc		Return status code
 *
 * Interprets a name of the form
 * "[settings_name/]tag_name[:type_name]" and fills in the appropriate
 * fields.
 */
static int parse_setting_name ( const char *name, struct settings **settings,
				unsigned int *tag,
				struct setting_type **type ) {
	char tmp_name[ strlen ( name ) + 1 ];
	char *settings_name;
	char *tag_name;
	char *type_name;
	struct named_setting *named_setting;
	char *tmp;

	/* Set defaults */
	*settings = NULL;
	*tag = 0;
	*type = &setting_type_hex;

	/* Split name into "[settings_name/]tag_name[:type_name]" */
	memcpy ( tmp_name, name, sizeof ( tmp_name ) );
	if ( ( tag_name = strchr ( tmp_name, '/' ) ) != NULL ) {
		*(tag_name++) = 0;
		settings_name = tmp_name;
	} else {
		tag_name = tmp_name;
		settings_name = NULL;
	}
	if ( ( type_name = strchr ( tag_name, ':' ) ) != NULL )
		*(type_name++) = 0;

	/* Identify settings block, if specified */
	if ( settings_name ) {
		*settings = find_settings ( settings_name );
		if ( *settings == NULL ) {
			DBG ( "Unrecognised settings block \"%s\" in \"%s\"\n",
			      settings_name, name );
			return -ENODEV;
		}
	}

	/* Identify tag number */
	if ( ( named_setting = find_named_setting ( tag_name ) ) != NULL ) {
		*tag = named_setting->tag;
		*type = named_setting->type;
	} else {
		/* Unrecognised name: try to interpret as a tag number */
		tmp = tag_name;
		while ( 1 ) {
			*tag = ( ( *tag << 8 ) | strtoul ( tmp, &tmp, 0 ) );
			if ( *tmp == 0 )
				break;
			if ( *tmp != '.' ) {
				DBG ( "Invalid tag number \"%s\" in \"%s\"\n",
				      tag_name, name );
				return -ENOENT;
			}
			tmp++;
		}
	}

	/* Identify setting type, if specified */
	if ( type_name ) {
		*type = find_setting_type ( type_name );
		if ( *type == NULL ) {
			DBG ( "Invalid setting type \"%s\" in \"%s\"\n",
			      type_name, name );
			return -ENOTSUP;
		}
	}

	return 0;
}

/**
 * Parse and set value of named setting
 *
 * @v name		Name of setting
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int set_named_setting ( const char *name, const char *value ) {
	struct settings *settings;
	unsigned int tag;
	struct setting_type *type;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &tag,
					 &type ) ) != 0 )
		return rc;
	if ( settings == NULL )
		return -ENODEV;
	return set_typed_setting ( settings, tag, type, value );
}

/**
 * Get and format value of named setting
 *
 * @v name		Name of setting
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
int get_named_setting ( const char *name, char *buf, size_t len ) {
	struct settings *settings;
	unsigned int tag;
	struct setting_type *type;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &tag,
					 &type ) ) != 0 )
		return rc;
	return get_typed_setting ( settings, tag, type, buf, len );
}

/******************************************************************************
 *
 * Setting types
 *
 ******************************************************************************
 */

/**
 * Parse and set value of string setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int setf_string ( struct settings *settings, unsigned int tag,
			 const char *value ) {
	return set_setting ( settings, tag, value, strlen ( value ) );
}

/**
 * Get and format value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int getf_string ( struct settings *settings, unsigned int tag,
			 char *buf, size_t len ) {
	return get_string_setting ( settings, tag, buf, len );
}

/** A string setting type */
struct setting_type setting_type_string __setting_type = {
	.name = "string",
	.setf = setf_string,
	.getf = getf_string,
};

/**
 * Parse and set value of IPv4 address setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int setf_ipv4 ( struct settings *settings, unsigned int tag,
		       const char *value ) {
	struct in_addr ipv4;

	if ( inet_aton ( value, &ipv4 ) == 0 )
		return -EINVAL;
	return set_setting ( settings, tag, &ipv4, sizeof ( ipv4 ) );
}

/**
 * Get and format value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int getf_ipv4 ( struct settings *settings, unsigned int tag,
		       char *buf, size_t len ) {
	struct in_addr ipv4;
	int rc;

	if ( ( rc = get_ipv4_setting ( settings, tag, &ipv4 ) ) < 0 )
		return rc;
	return snprintf ( buf, len, inet_ntoa ( ipv4 ) );
}

/** An IPv4 address setting type */
struct setting_type setting_type_ipv4 __setting_type = {
	.name = "ipv4",
	.setf = setf_ipv4,
	.getf = getf_ipv4,
};

/**
 * Parse and set value of integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int setf_int ( struct settings *settings, unsigned int tag,
		      const char *value, unsigned int size ) {
	union {
		uint32_t num;
		uint8_t bytes[4];
	} u;
	char *endp;

	u.num = htonl ( strtoul ( value, &endp, 0 ) );
	if ( *endp )
		return -EINVAL;
	return set_setting ( settings, tag, 
			     &u.bytes[ sizeof ( u ) - size ], size );
}

/**
 * Parse and set value of 8-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int setf_int8 ( struct settings *settings, unsigned int tag,
		       const char *value ) {
	return setf_int ( settings, tag, value, 1 );
}

/**
 * Parse and set value of 16-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int setf_int16 ( struct settings *settings, unsigned int tag,
		       const char *value ) {
	return setf_int ( settings, tag, value, 2 );
}

/**
 * Parse and set value of 32-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int setf_int32 ( struct settings *settings, unsigned int tag,
		       const char *value ) {
	return setf_int ( settings, tag, value, 4 );
}

/**
 * Get and format value of signed integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int getf_int ( struct settings *settings, unsigned int tag,
		      char *buf, size_t len ) {
	long value;
	int rc;

	if ( ( rc = get_int_setting ( settings, tag, &value ) ) < 0 )
		return rc;
	return snprintf ( buf, len, "%ld", value );
}

/**
 * Get and format value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int getf_uint ( struct settings *settings, unsigned int tag,
		       char *buf, size_t len ) {
	unsigned long value;
	int rc;

	if ( ( rc = get_uint_setting ( settings, tag, &value ) ) < 0 )
		return rc;
	return snprintf ( buf, len, "%#lx", value );
}

/** A signed 8-bit integer setting type */
struct setting_type setting_type_int8 __setting_type = {
	.name = "int8",
	.setf = setf_int8,
	.getf = getf_int,
};

/** A signed 16-bit integer setting type */
struct setting_type setting_type_int16 __setting_type = {
	.name = "int16",
	.setf = setf_int16,
	.getf = getf_int,
};

/** A signed 32-bit integer setting type */
struct setting_type setting_type_int32 __setting_type = {
	.name = "int32",
	.setf = setf_int32,
	.getf = getf_int,
};

/** An unsigned 8-bit integer setting type */
struct setting_type setting_type_uint8 __setting_type = {
	.name = "uint8",
	.setf = setf_int8,
	.getf = getf_uint,
};

/** An unsigned 16-bit integer setting type */
struct setting_type setting_type_uint16 __setting_type = {
	.name = "uint16",
	.setf = setf_int16,
	.getf = getf_uint,
};

/** An unsigned 32-bit integer setting type */
struct setting_type setting_type_uint32 __setting_type = {
	.name = "uint32",
	.setf = setf_int32,
	.getf = getf_uint,
};

/**
 * Parse and set value of hex string setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int setf_hex ( struct settings *settings, unsigned int tag,
		      const char *value ) {
	char *ptr = ( char * ) value;
	uint8_t bytes[ strlen ( value ) ]; /* cannot exceed strlen(value) */
	unsigned int len = 0;

	while ( 1 ) {
		bytes[len++] = strtoul ( ptr, &ptr, 16 );
		switch ( *ptr ) {
		case '\0' :
			return set_setting ( settings, tag, bytes, len );
		case ':' :
			ptr++;
			break;
		default :
			return -EINVAL;
		}
	}
}

/**
 * Get and format value of hex string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int getf_hex ( struct settings *settings, unsigned int tag,
		      char *buf, size_t len ) {
	int raw_len;
	int check_len;
	int used = 0;
	int i;

	raw_len = get_setting_len ( settings, tag );
	if ( raw_len < 0 )
		return raw_len;

	{
		uint8_t raw[raw_len];

		check_len = get_setting ( settings, tag, raw, sizeof ( raw ) );
		assert ( check_len == raw_len );
		
		if ( len )
			buf[0] = 0; /* Ensure that a terminating NUL exists */
		for ( i = 0 ; i < raw_len ; i++ ) {
			used += ssnprintf ( ( buf + used ), ( len - used ),
					    "%s%02x", ( used ? ":" : "" ),
					    raw[i] );
		}
		return used;
	}
}

/** A hex-string setting */
struct setting_type setting_type_hex __setting_type = {
	.name = "hex",
	.setf = setf_hex,
	.getf = getf_hex,
};

/******************************************************************************
 *
 * Named settings
 *
 ******************************************************************************
 */

/** Some basic setting definitions */
struct named_setting basic_named_settings[] __named_setting = {
	{
		.name = "ip",
		.description = "IPv4 address of this interface",
		.tag = DHCP_EB_YIADDR,
		.type = &setting_type_ipv4,
	},
	{
		.name = "subnet-mask",
		.description = "IPv4 subnet mask",
		.tag = DHCP_SUBNET_MASK,
		.type = &setting_type_ipv4,
	},
	{
		.name = "routers",
		.description = "Default gateway",
		.tag = DHCP_ROUTERS,
		.type = &setting_type_ipv4,
	},
	{
		.name = "domain-name-servers",
		.description = "DNS server",
		.tag = DHCP_DNS_SERVERS,
		.type = &setting_type_ipv4,
	},
	{
		.name = "hostname",
		.description = "Host name of this machine",
		.tag = DHCP_HOST_NAME,
		.type = &setting_type_string,
	},
	{
		.name = "filename",
		.description = "Boot filename",
		.tag = DHCP_BOOTFILE_NAME,
		.type = &setting_type_string,
	},
	{
		.name = "root-path",
		.description = "NFS/iSCSI root path",
		.tag = DHCP_ROOT_PATH,
		.type = &setting_type_string,
	},
	{
		.name = "username",
		.description = "User name for authentication",
		.tag = DHCP_EB_USERNAME,
		.type = &setting_type_string,
	},
	{
		.name = "password",
		.description = "Password for authentication",
		.tag = DHCP_EB_PASSWORD,
		.type = &setting_type_string,
	},
	{
		.name = "initiator-iqn",
		.description = "iSCSI initiator name",
		.tag = DHCP_ISCSI_INITIATOR_IQN,
		.type = &setting_type_string,
	},
	{
		.name = "priority",
		.description = "Priority of these options",
		.tag = DHCP_EB_PRIORITY,
		.type = &setting_type_int8,
	},
};
