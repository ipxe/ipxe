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
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <vsprintf.h>
#include <gpxe/in.h>
#include <gpxe/settings.h>

/** @file
 *
 * Configuration settings
 *
 */

/** Registered configuration setting types */
static struct config_setting_type
config_setting_types[0] __table_start ( config_setting_types );
static struct config_setting_type
config_setting_types_end[0] __table_end ( config_setting_types );

/** Registered configuration settings */
static struct config_setting
config_settings[0] __table_start ( config_settings );
static struct config_setting
config_settings_end[0] __table_end ( config_settings );

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
		if ( strcmp ( name, type->name ) == 0 )
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
		if ( strcmp ( name, setting->name ) == 0 )
			return setting;
	}
	return NULL;
}

/**
 * Find or build configuration setting
 *
 * @v name		Name
 * @v tmp_setting	Temporary buffer for constructing a setting
 * @ret setting		Configuration setting, or NULL
 *
 * Find setting if it exists.  If it doesn't exist, but the name is of
 * the form "<num>.<type>" (e.g. "12.string"), then construct a
 * setting for that tag and data type, and return it.  The constructed
 * setting will be placed in the temporary buffer.
 */
static struct config_setting *
find_or_build_config_setting ( const char *name,
			       struct config_setting *tmp_setting ) {
	struct config_setting *setting;
	char *separator;

	/* Look in the list of registered settings first */
	setting = find_config_setting ( name );
	if ( setting )
		return setting;

	/* If name is of the form "<num>.<type>", try to construct a setting */
	setting = tmp_setting;
	memset ( setting, 0, sizeof ( *setting ) );
	setting->name = name;
	setting->tag = strtoul ( name, &separator, 10 );
	if ( *separator != '.' )
		return NULL;
	setting->type = find_config_setting_type ( separator + 1 );
	if ( ! setting->type )
		return NULL;
	return setting;
}

/**
 * Show value of setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
int show_setting ( struct config_context *context, const char *name,
		   char *buf, size_t len ) {
	struct config_setting *setting;
	struct config_setting tmp_setting;

	setting = find_or_build_config_setting ( name, &tmp_setting );
	if ( ! setting )
		return -ENOENT;
	return setting->type->show ( context, setting, buf, len );
}

/**
 * Set value of setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */
int set_setting ( struct config_context *context, const char *name,
		  const char *value ) {
	struct config_setting *setting;
	struct config_setting tmp_setting;

	setting = find_or_build_config_setting ( name, &tmp_setting );
	if ( ! setting )
		return -ENOENT;
	return setting->type->set ( context, setting, value );
}

/**
 * Clear setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @ret rc		Return status code
 */
int clear_setting ( struct config_context *context, const char *name ) {
	struct config_setting *setting;
	struct config_setting tmp_setting;

	setting = find_or_build_config_setting ( name, &tmp_setting );
	if ( ! setting )
		return -ENOENT;

	/* All types of settings get cleared the same way */
	delete_dhcp_option ( context->options, setting->tag );

	return 0;
}

/**
 * Show value of string setting
 *
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
static int show_string ( struct config_context *context,
			 struct config_setting *setting,
			 char *buf, size_t len ) {
	struct dhcp_option *option;

	option = find_dhcp_option ( context->options, setting->tag );
	if ( ! option )
		return -ENODATA;
	dhcp_snprintf ( buf, len, option );
	return 0;
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
		return -ENOMEM;
	return 0;
}

/** A string configuration setting */
struct config_setting_type config_setting_type_string __config_setting_type = {
	.name = "string",
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
 * @ret rc		Return status code
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
	snprintf ( buf, len, inet_ntoa ( ipv4 ) );
	return 0;
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
	int rc;
	
	if ( ( rc = inet_aton ( value, &ipv4 ) ) == 0 )
		return rc;
	option = set_dhcp_option ( context->options, setting->tag,
				   &ipv4, sizeof ( ipv4 ) );
	if ( ! option )
		return -ENOMEM;
	return 0;
}

/** An IPv4 configuration setting */
struct config_setting_type config_setting_type_ipv4 __config_setting_type = {
	.name = "ipv4",
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
 * @ret rc		Return status code
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
	snprintf ( buf, len, "%ld", num );
	return 0;
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
		return -ENOMEM;
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
	return set_int ( context, setting, value, 8 );
}

/** An 8-bit integer configuration setting */
struct config_setting_type config_setting_type_int8 __config_setting_type = {
	.name = "int8",
	.show = show_int,
	.set = set_int8,
};

/** Some basic setting definitions */
struct config_setting ip_config_setting __config_setting = {
	.name = "ip",
	.tag = DHCP_EB_YIADDR,
	.type = &config_setting_type_ipv4,
};
struct config_setting hostname_config_setting __config_setting = {
	.name = "hostname",
	.tag = DHCP_HOST_NAME,
	.type = &config_setting_type_string,
};
struct config_setting username_config_setting __config_setting = {
	.name = "username",
	.tag = DHCP_EB_USERNAME,
	.type = &config_setting_type_string,
};
struct config_setting password_config_setting __config_setting = {
	.name = "password",
	.tag = DHCP_EB_PASSWORD,
	.type = &config_setting_type_string,
};
struct config_setting root_path_config_setting __config_setting = {
	.name = "root-path",
	.tag = DHCP_ROOT_PATH,
	.type = &config_setting_type_string,
};
struct config_setting priority_config_setting __config_setting = {
	.name = "priority",
	.tag = DHCP_EB_PRIORITY,
	.type = &config_setting_type_int8,
};
