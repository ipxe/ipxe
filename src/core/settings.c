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
#include <errno.h>
#include <assert.h>
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

/** Show value of setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v buf		Buffer to contain value
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
int ( show_setting ) ( struct config_context *context, const char *name,
		       char *buf, size_t len ) {
	struct config_setting *setting;
	struct config_setting tmp_setting;

	setting = find_or_build_config_setting ( name, &tmp_setting );
	if ( ! setting )
		return -ENOENT;
	return setting->type->show ( context, setting, buf, len );
}

/** Set value of setting
 *
 * @v context		Configuration context
 * @v name		Configuration setting name
 * @v value		Setting value (as a string)
 * @ret rc		Return status code
 */
int ( set_setting ) ( struct config_context *context, const char *name,
		      const char *value ) {
	struct config_setting *setting;
	struct config_setting tmp_setting;

	setting = find_or_build_config_setting ( name, &tmp_setting );
	if ( ! setting )
		return -ENOENT;
	return setting->type->set ( context, setting, value );
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
		return -ENOENT;
	dhcp_snprintf ( buf, len, option );
	return 0;
}

/** Set value of string setting
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
