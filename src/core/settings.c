/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <gpxe/settings.h>

/** @file
 *
 * Configuration settings
 *
 */

/** Registered configuration setting types */
static struct config_setting_type config_setting_types[0]
	__table_start ( struct config_setting_type, config_setting_types );
static struct config_setting_type config_setting_types_end[0]
	__table_end ( struct config_setting_type, config_setting_types );

/** Registered configuration settings */
static struct config_setting config_settings[0]
	__table_start ( struct config_setting, config_settings );
static struct config_setting config_settings_end[0]
	__table_end ( struct config_setting, config_settings );

struct config_setting_type config_setting_type_hex __config_setting_type;

/**
 * Find configuration setting type
 *
 * @v name		Name
 * @ret type		Configuration setting type, or NULL
 */
static struct config_setting_type *
find_config_setting_type ( const char *name ) {
	struct config_setting_type *type;

	for ( type = config_setting_types ; type < config_setting_types_end ;
	      type++ ) {
		if ( strcasecmp ( name, type->name ) == 0 )
			return type;
	}
	return NULL;
}

/**
 * Find configuration setting
 *
 * @v name		Name
 * @ret setting		Configuration setting, or NULL
 */
static struct config_setting * find_config_setting ( const char *name ) {
	struct config_setting *setting;

	for ( setting = config_settings ; setting < config_settings_end ;
	      setting++ ) {
		if ( strcasecmp ( name, setting->name ) == 0 )
			return setting;
	}
	return NULL;
}

/**
 * Find or build configuration setting
 *
 * @v name		Name
 * @v setting		Buffer to fill in with setting
 * @ret rc		Return status code
 *
 * Find setting if it exists.  If it doesn't exist, but the name is of
 * the form "<num>:<type>" (e.g. "12:string"), then construct a
 * setting for that tag and data type, and return it.  The constructed
 * setting will be placed in the buffer.
 */
static int find_or_build_config_setting ( const char *name,
					  struct config_setting *setting ) {
	struct config_setting *known_setting;
	char tmp_name[ strlen ( name ) + 1 ];
	char *qualifier;
	char *tmp;

	/* Set defaults */
	memset ( setting, 0, sizeof ( *setting ) );
	setting->name = name;
	setting->type = &config_setting_type_hex;

	/* Strip qualifier, if present */
	memcpy ( tmp_name, name, sizeof ( tmp_name ) );
	if ( ( qualifier = strchr ( tmp_name, ':' ) ) != NULL )
		*(qualifier++) = 0;

	/* If we recognise the name of the setting, use it */
	if ( ( known_setting = find_config_setting ( tmp_name ) ) != NULL ) {
		memcpy ( setting, known_setting, sizeof ( *setting ) );
	} else {
		/* Otherwise, try to interpret as a numerical setting */
		for ( tmp = tmp_name ; 1 ; tmp++ ) {
			setting->tag = ( ( setting->tag << 8 ) |
					 strtoul ( tmp, &tmp, 0 ) );
			if ( *tmp != '.' )
				break;
		}
		if ( *tmp != 0 )
			return -EINVAL;
	}

	/* Apply qualifier, if present */
	if ( qualifier ) {
		setting->type = find_config_setting_type ( qualifier );
		if ( ! setting->type )
			return -EINVAL;
	}

	return 0;
}

/**
 * Show value of named setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
int show_named_setting ( struct config_context *context, const char *name,
			 char *buf, size_t len ) {
	struct config_setting setting;
	int rc;

	if ( ( rc = find_or_build_config_setting ( name, &setting ) ) != 0 )
		return rc;
	return show_setting ( context, &setting, buf, len );
}

/**
 * Set value of named setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */
int set_named_setting ( struct config_context *context, const char *name,
			const char *value ) {
	struct config_setting setting;
	int rc;

	if ( ( rc = find_or_build_config_setting ( name, &setting ) ) != 0 )
		return rc;
	return set_setting ( context, &setting, value );
}

/**
 * Set value of setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string), or NULL
 * @ret rc		Return status code
 */
int set_setting ( struct config_context *context,
		  struct config_setting *setting,
		  const char *value ) {
	if ( ( ! value ) || ( ! *value ) ) {
		/* Save putting deletion logic in each individual handler */
		return clear_setting ( context, setting );
	}
	return setting->type->set ( context, setting, value );
}

/**
 * Show value of string setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int show_string ( struct config_context *context,
			 struct config_setting *setting,
			 char *buf, size_t len ) {
	struct dhcp_option *option;

	option = find_dhcp_option ( context->options, setting->tag );
	if ( ! option )
		return -ENODATA;
	return dhcp_snprintf ( buf, len, option );
}

/**
 * Set value of string setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */ 
static int set_string ( struct config_context *context,
			struct config_setting *setting,
			const char *value ) {
	struct dhcp_option *option;

	option = set_dhcp_option ( context->options, setting->tag,
				   value, strlen ( value ) );
	if ( ! option )
		return -ENOSPC;
	return 0;
}

/** A string configuration setting */
struct config_setting_type config_setting_type_string __config_setting_type = {
	.name = "string",
	.description = "Text string",
	.show = show_string,
	.set = set_string,
};

/**
 * Show value of IPv4 setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int show_ipv4 ( struct config_context *context,
		       struct config_setting *setting,
		       char *buf, size_t len ) {
	struct dhcp_option *option;
	struct in_addr ipv4;

	option = find_dhcp_option ( context->options, setting->tag );
	if ( ! option )
		return -ENODATA;
	dhcp_ipv4_option ( option, &ipv4 );
	return snprintf ( buf, len, inet_ntoa ( ipv4 ) );
}

/**
 * Set value of IPV4 setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */ 
static int set_ipv4 ( struct config_context *context,
		      struct config_setting *setting,
		      const char *value ) {
	struct dhcp_option *option;
	struct in_addr ipv4;
	
	if ( inet_aton ( value, &ipv4 ) == 0 )
		return -EINVAL;
	option = set_dhcp_option ( context->options, setting->tag,
				   &ipv4, sizeof ( ipv4 ) );
	if ( ! option )
		return -ENOSPC;
	return 0;
}

/** An IPv4 configuration setting */
struct config_setting_type config_setting_type_ipv4 __config_setting_type = {
	.name = "ipv4",
	.description = "IPv4 address",
	.show = show_ipv4,
	.set = set_ipv4,
};

/**
 * Show value of integer setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int show_int ( struct config_context *context,
		      struct config_setting *setting,
		      char *buf, size_t len ) {
	struct dhcp_option *option;
	long num;

	option = find_dhcp_option ( context->options, setting->tag );
	if ( ! option )
		return -ENODATA;
	num = dhcp_num_option ( option );
	return snprintf ( buf, len, "%ld", num );
}

/**
 * Set value of integer setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @v size		Size of integer (in bytes)
 * @ret rc		Return status code
 */ 
static int set_int ( struct config_context *context,
		     struct config_setting *setting,
		     const char *value, unsigned int size ) {
	struct dhcp_option *option;
	union {
		uint32_t num;
		uint8_t bytes[4];
	} u;
	char *endp;

	/* Parse number */
	if ( ! *value )
		return -EINVAL;
	u.num = htonl ( strtoul ( value, &endp, 0 ) );
	if ( *endp )
		return -EINVAL;

	/* Set option */
	option = set_dhcp_option ( context->options, setting->tag,
				   &u.bytes[ sizeof ( u ) - size ], size );
	if ( ! option )
		return -ENOSPC;
	return 0;
}

/**
 * Set value of 8-bit integer setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @v size		Size of integer (in bytes)
 * @ret rc		Return status code
 */ 
static int set_int8 ( struct config_context *context,
		      struct config_setting *setting,
		      const char *value ) {
	return set_int ( context, setting, value, 1 );
}

/**
 * Set value of 16-bit integer setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @v size		Size of integer (in bytes)
 * @ret rc		Return status code
 */ 
static int set_int16 ( struct config_context *context,
		       struct config_setting *setting,
		       const char *value ) {
	return set_int ( context, setting, value, 2 );
}

/**
 * Set value of 32-bit integer setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @v size		Size of integer (in bytes)
 * @ret rc		Return status code
 */ 
static int set_int32 ( struct config_context *context,
		       struct config_setting *setting,
		       const char *value ) {
	return set_int ( context, setting, value, 4 );
}

/** An 8-bit integer configuration setting */
struct config_setting_type config_setting_type_int8 __config_setting_type = {
	.name = "int8",
	.description = "8-bit integer",
	.show = show_int,
	.set = set_int8,
};

/** A 16-bit integer configuration setting */
struct config_setting_type config_setting_type_int16 __config_setting_type = {
	.name = "int16",
	.description = "16-bit integer",
	.show = show_int,
	.set = set_int16,
};

/** A 32-bit integer configuration setting */
struct config_setting_type config_setting_type_int32 __config_setting_type = {
	.name = "int32",
	.description = "32-bit integer",
	.show = show_int,
	.set = set_int32,
};

/**
 * Set value of hex-string setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */ 
static int set_hex ( struct config_context *context,
		     struct config_setting *setting,
		     const char *value ) {
	struct dhcp_option *option;
	char *ptr = ( char * ) value;
	uint8_t bytes[ strlen ( value ) ]; /* cannot exceed strlen(value) */
	unsigned int len = 0;

	while ( 1 ) {
		bytes[len++] = strtoul ( ptr, &ptr, 16 );
		switch ( *ptr ) {
		case '\0' :
			option = set_dhcp_option ( context->options,
						   setting->tag, bytes, len );
			if ( ! option )
				return -ENOSPC;
			return 0;
		case ':' :
			ptr++;
			break;
		default :
			return -EINVAL;
		}
	}
}

/**
 * Show value of hex-string setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int show_hex ( struct config_context *context,
		      struct config_setting *setting,
		      char *buf, size_t len ) {
	struct dhcp_option *option;
	int used = 0;
	int i;

	option = find_dhcp_option ( context->options, setting->tag );
	if ( ! option )
		return -ENODATA;

	for ( i = 0 ; i < option->len ; i++ ) {
		used += ssnprintf ( ( buf + used ), ( len - used ),
				    "%s%02x", ( used ? ":" : "" ),
				    option->data.bytes[i] );
	}
	return used;
}

/** A hex-string configuration setting */
struct config_setting_type config_setting_type_hex __config_setting_type = {
	.name = "hex",
	.description = "Hex string",
	.show = show_hex,
	.set = set_hex,
};

/** Some basic setting definitions */
struct config_setting basic_config_settings[] __config_setting = {
	{
		.name = "ip",
		.description = "IP address of this machine (e.g. 192.168.0.1)",
		.tag = DHCP_EB_YIADDR,
		.type = &config_setting_type_ipv4,
	},
	{
		.name = "hostname",
		.description = "Host name of this machine",
		.tag = DHCP_HOST_NAME,
		.type = &config_setting_type_string,
	},
	{
		.name = "username",
		.description = "User name for authentication to servers",
		.tag = DHCP_EB_USERNAME,
		.type = &config_setting_type_string,
	},
	{
		.name = "password",
		.description = "Password for authentication to servers",
		.tag = DHCP_EB_PASSWORD,
		.type = &config_setting_type_string,
	},
	{
		.name = "root-path",
		.description = "NFS/iSCSI root path",
		.tag = DHCP_ROOT_PATH,
		.type = &config_setting_type_string,
	},
	{
		.name = "priority",
		.description = "Priority of these options",
		.tag = DHCP_EB_PRIORITY,
		.type = &config_setting_type_int8,
	},
	{
		.name = "initiator-iqn",
		.description = "iSCSI qualified name of this machine",
		.tag = DHCP_ISCSI_INITIATOR_IQN,
		.type = &config_setting_type_string,
	}
};
