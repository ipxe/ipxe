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
#include <gpxe/uuid.h>
#include <gpxe/uri.h>
#include <gpxe/settings.h>

/** @file
 *
 * Configuration settings
 *
 */

/** Registered settings */
static struct setting settings[0]
	__table_start ( struct setting, settings );
static struct setting settings_end[0]
	__table_end ( struct setting, settings );

/** Registered setting types */
static struct setting_type setting_types[0]
	__table_start ( struct setting_type, setting_types );
static struct setting_type setting_types_end[0]
	__table_end ( struct setting_type, setting_types );

/** Registered settings applicators */
static struct settings_applicator settings_applicators[0]
	__table_start ( struct settings_applicator, settings_applicators );
static struct settings_applicator settings_applicators_end[0]
	__table_end ( struct settings_applicator, settings_applicators );

/******************************************************************************
 *
 * Registered settings blocks
 *
 ******************************************************************************
 */

/**
 * Store value of simple setting
 *
 * @v options		DHCP option block
 * @v setting		Setting to store
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
int simple_settings_store ( struct settings *settings, struct setting *setting,
			    const void *data, size_t len ) {
	struct simple_settings *simple =
		container_of ( settings, struct simple_settings, settings );
	return dhcpopt_extensible_store ( &simple->dhcpopts, setting->tag,
					  data, len );
}

/**
 * Fetch value of simple setting
 *
 * @v options		DHCP option block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
int simple_settings_fetch ( struct settings *settings, struct setting *setting,
			    void *data, size_t len ) {
	struct simple_settings *simple =
		container_of ( settings, struct simple_settings, settings );
	return dhcpopt_fetch ( &simple->dhcpopts, setting->tag, data, len );
}

/** Simple settings operations */
struct settings_operations simple_settings_operations = {
	.store = simple_settings_store,
	.fetch = simple_settings_fetch,
};

/** Root simple settings block */
struct simple_settings simple_settings_root = {
	.settings = {
		.refcnt = NULL,
		.name = "",
		.siblings =
		     LIST_HEAD_INIT ( simple_settings_root.settings.siblings ),
		.children =
		     LIST_HEAD_INIT ( simple_settings_root.settings.children ),
		.op = &simple_settings_operations,
	},
};

/** Root settings block */
#define settings_root simple_settings_root.settings

/**
 * Apply all settings
 *
 * @ret rc		Return status code
 */
static int apply_settings ( void ) {
	struct settings_applicator *applicator;
	int rc;

	/* Call all settings applicators */
	for ( applicator = settings_applicators ;
	      applicator < settings_applicators_end ; applicator++ ) {
		if ( ( rc = applicator->apply() ) != 0 ) {
			DBG ( "Could not apply settings using applicator "
			      "%p: %s\n", applicator, strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/**
 * Reprioritise settings
 *
 * @v settings		Settings block
 *
 * Reorders the settings block amongst its siblings according to its
 * priority.
 */
static void reprioritise_settings ( struct settings *settings ) {
	struct settings *parent = settings->parent;
	long priority;
	struct settings *tmp;
	long tmp_priority;

	/* Stop when we reach the top of the tree */
	if ( ! parent )
		return;

	/* Read priority, if present */
	priority = fetch_intz_setting ( settings, &priority_setting );

	/* Remove from siblings list */
	list_del ( &settings->siblings );

	/* Reinsert after any existing blocks which have a higher priority */
	list_for_each_entry ( tmp, &parent->children, siblings ) {
		tmp_priority = fetch_intz_setting ( tmp, &priority_setting );
		if ( priority > tmp_priority )
			break;
	}
	list_add_tail ( &settings->siblings, &tmp->siblings );

	/* Recurse up the tree */
	reprioritise_settings ( parent );
}

/**
 * Register settings block
 *
 * @v settings		Settings block
 * @v parent		Parent settings block, or NULL
 * @ret rc		Return status code
 */
int register_settings ( struct settings *settings, struct settings *parent ) {
	struct settings *old_settings;

	/* NULL parent => add to settings root */
	assert ( settings != NULL );
	if ( parent == NULL )
		parent = &settings_root;

	/* Remove any existing settings with the same name */
	if ( ( old_settings = find_child_settings ( parent, settings->name ) ))
		unregister_settings ( old_settings );

	/* Add to list of settings */
	ref_get ( settings->refcnt );
	ref_get ( parent->refcnt );
	settings->parent = parent;
	list_add_tail ( &settings->siblings, &parent->children );
	DBGC ( settings, "Settings %p registered\n", settings );

	/* Fix up settings priority */
	reprioritise_settings ( settings );

	/* Apply potentially-updated settings */
	apply_settings();

	return 0;
}

/**
 * Unregister settings block
 *
 * @v settings		Settings block
 */
void unregister_settings ( struct settings *settings ) {

	/* Remove from list of settings */
	ref_put ( settings->refcnt );
	ref_put ( settings->parent->refcnt );
	settings->parent = NULL;
	list_del ( &settings->siblings );
	DBGC ( settings, "Settings %p unregistered\n", settings );

	/* Apply potentially-updated settings */
	apply_settings();
}

/**
 * Find child named settings block
 *
 * @v parent		Parent settings block
 * @v name		Name within this parent
 * @ret settings	Settings block, or NULL
 */
struct settings * find_child_settings ( struct settings *parent,
					const char *name ) {
	struct settings *settings;
	size_t len;

	/* NULL parent => add to settings root */
	if ( parent == NULL )
		parent = &settings_root;

	/* Look for a child whose name matches the initial component */
	list_for_each_entry ( settings, &parent->children, siblings ) {
		len = strlen ( settings->name );
		if ( strncmp ( name, settings->name, len ) != 0 )
			continue;
		if ( name[len] == 0 )
			return settings;
		if ( name[len] == '.' )
			return find_child_settings ( settings,
						     ( name + len + 1 ) );
	}

	return NULL;
}

/**
 * Find named settings block
 *
 * @v name		Name
 * @ret settings	Settings block, or NULL
 */
struct settings * find_settings ( const char *name ) {

	/* If name is empty, use the root */
	if ( ! *name )
		return &settings_root;

	return find_child_settings ( &settings_root, name );
}

/******************************************************************************
 *
 * Core settings routines
 *
 ******************************************************************************
 */

/**
 * Store value of setting
 *
 * @v settings		Settings block, or NULL
 * @v setting		Setting to store
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
int store_setting ( struct settings *settings, struct setting *setting,
		    const void *data, size_t len ) {
	int rc;

	/* NULL settings implies storing into the global settings root */
	if ( ! settings )
		settings = &settings_root;

	/* Store setting */
	if ( ( rc = settings->op->store ( settings, setting,
					  data, len ) ) != 0 )
		return rc;

	/* Reprioritise settings if necessary */
	if ( setting_cmp ( setting, &priority_setting ) == 0 )
		reprioritise_settings ( settings );

	/* If these settings are registered, apply potentially-updated
	 * settings
	 */
	for ( ; settings ; settings = settings->parent ) {
		if ( settings == &settings_root ) {
			if ( ( rc = apply_settings() ) != 0 )
				return rc;
			break;
		}
	}

	return 0;
}

/**
 * Fetch value of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 *
 * The actual length of the setting will be returned even if
 * the buffer was too small.
 */
int fetch_setting ( struct settings *settings, struct setting *setting,
		    void *data, size_t len ) {
	struct settings *child;
	int ret;

	/* Avoid returning uninitialised data on error */
	memset ( data, 0, len );

	/* NULL settings implies starting at the global settings root */
	if ( ! settings )
		settings = &settings_root;

	/* Try this block first */
	if ( ( ret = settings->op->fetch ( settings, setting,
					   data, len ) ) >= 0 )
		return ret;

	/* Recurse into each child block in turn */
	list_for_each_entry ( child, &settings->children, siblings ) {
		if ( ( ret = fetch_setting ( child, setting,
					     data, len ) ) >= 0 )
			return ret;
	}

	return -ENOENT;
}

/**
 * Fetch length of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @ret len		Length of setting data, or negative error
 *
 * This function can also be used as an existence check for the
 * setting.
 */
int fetch_setting_len ( struct settings *settings, struct setting *setting ) {
	return fetch_setting ( settings, setting, NULL, 0 );
}

/**
 * Fetch value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting string data
 * @v len		Length of buffer
 * @ret len		Length of string setting, or negative error
 *
 * The resulting string is guaranteed to be correctly NUL-terminated.
 * The returned length will be the length of the underlying setting
 * data.
 */
int fetch_string_setting ( struct settings *settings, struct setting *setting,
			   char *data, size_t len ) {
	memset ( data, 0, len );
	return fetch_setting ( settings, setting, data,
			       ( ( len > 0 ) ? ( len - 1 ) : 0 ) );
}

/**
 * Fetch value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to allocate and fill with setting string data
 * @ret len		Length of string setting, or negative error
 *
 * The resulting string is guaranteed to be correctly NUL-terminated.
 * The returned length will be the length of the underlying setting
 * data.  The caller is responsible for eventually freeing the
 * allocated buffer.
 */
int fetch_string_setting_copy ( struct settings *settings,
				struct setting *setting,
				char **data ) {
	int len;
	int check_len;

	len = fetch_setting_len ( settings, setting );
	if ( len < 0 )
		return len;

	*data = malloc ( len + 1 );
	if ( ! *data )
		return -ENOMEM;

	fetch_string_setting ( settings, setting, *data, ( len + 1 ) );
	assert ( check_len == len );
	return len;
}

/**
 * Fetch value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v inp		IPv4 address to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_ipv4_setting ( struct settings *settings, struct setting *setting,
			 struct in_addr *inp ) {
	int len;

	len = fetch_setting ( settings, setting, inp, sizeof ( *inp ) );
	if ( len < 0 )
		return len;
	if ( len < ( int ) sizeof ( *inp ) )
		return -ERANGE;
	return len;
}

/**
 * Fetch value of signed integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_int_setting ( struct settings *settings, struct setting *setting,
			long *value ) {
	union {
		uint8_t u8[ sizeof ( long ) ];
		int8_t s8[ sizeof ( long ) ];
	} buf;
	int len;
	int i;

	/* Avoid returning uninitialised data on error */
	*value = 0;

	/* Fetch raw (network-ordered, variable-length) setting */
	len = fetch_setting ( settings, setting, &buf, sizeof ( buf ) );
	if ( len < 0 )
		return len;
	if ( len > ( int ) sizeof ( buf ) )
		return -ERANGE;

	/* Convert to host-ordered signed long */
	*value = ( ( buf.s8[0] >= 0 ) ? 0 : -1L );
	for ( i = 0 ; i < len ; i++ ) {
		*value = ( ( *value << 8 ) | buf.u8[i] );
	}

	return len;
}

/**
 * Fetch value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_uint_setting ( struct settings *settings, struct setting *setting,
			 unsigned long *value ) {
	long svalue;
	int len;

	/* Avoid returning uninitialised data on error */
	*value = 0;

	/* Fetch as a signed long */
	len = fetch_int_setting ( settings, setting, &svalue );
	if ( len < 0 )
		return len;

	/* Mask off sign-extended bits */
	*value = ( svalue & ( -1UL >> ( sizeof ( long ) - len ) ) );

	return len;
}

/**
 * Fetch value of signed integer setting, or zero
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @ret value		Setting value, or zero
 */
long fetch_intz_setting ( struct settings *settings, struct setting *setting ){
	long value;

	fetch_int_setting ( settings, setting, &value );
	return value;
}

/**
 * Fetch value of unsigned integer setting, or zero
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @ret value		Setting value, or zero
 */
unsigned long fetch_uintz_setting ( struct settings *settings,
				    struct setting *setting ) {
	unsigned long value;

	fetch_uint_setting ( settings, setting, &value );
	return value;
}

/**
 * Fetch value of UUID setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v uuid		UUID to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_uuid_setting ( struct settings *settings, struct setting *setting,
			 union uuid *uuid ) {
	int len;

	len = fetch_setting ( settings, setting, uuid, sizeof ( *uuid ) );
	if ( len < 0 )
		return len;
	if ( len != sizeof ( *uuid ) )
		return -ERANGE;
	return len;
}

/**
 * Compare two settings
 *
 * @v a			Setting to compare
 * @v b			Setting to compare
 * @ret 0		Settings are the same
 * @ret non-zero	Settings are not the same
 */
int setting_cmp ( struct setting *a, struct setting *b ) {

	/* If the settings have tags, compare them */
	if ( a->tag && ( a->tag == b->tag ) )
		return 0;

	/* Otherwise, compare the names */
	return strcmp ( a->name, b->name );
}

/******************************************************************************
 *
 * Formatted setting routines
 *
 ******************************************************************************
 */

/**
 * Store value of typed setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v type		Settings type
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int storef_setting ( struct settings *settings, struct setting *setting,
		     const char *value ) {

	/* NULL value implies deletion.  Avoid imposing the burden of
	 * checking for NULL values on each typed setting's storef()
	 * method.
	 */
	if ( ! value )
		return delete_setting ( settings, setting );
		
	return setting->type->storef ( settings, setting, value );
}

/**
 * Find named setting
 *
 * @v name		Name
 * @ret setting		Named setting, or NULL
 */
static struct setting * find_setting ( const char *name ) {
	struct setting *setting;

	for ( setting = settings ; setting < settings_end ; setting++ ) {
		if ( strcmp ( name, setting->name ) == 0 )
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
		if ( strcmp ( name, type->name ) == 0 )
			return type;
	}
	return NULL;
}

/**
 * Parse setting name
 *
 * @v name		Name of setting
 * @v settings		Settings block to fill in
 * @v setting		Setting to fill in
 * @ret rc		Return status code
 *
 * Interprets a name of the form
 * "[settings_name/]tag_name[:type_name]" and fills in the appropriate
 * fields.
 */
static int parse_setting_name ( const char *name, struct settings **settings,
				struct setting *setting ) {
	char tmp_name[ strlen ( name ) + 1 ];
	char *settings_name;
	char *setting_name;
	char *type_name;
	struct setting *named_setting;
	char *tmp;

	/* Set defaults */
	*settings = &settings_root;
	memset ( setting, 0, sizeof ( *setting ) );
	setting->type = &setting_type_hex;

	/* Split name into "[settings_name/]setting_name[:type_name]" */
	memcpy ( tmp_name, name, sizeof ( tmp_name ) );
	if ( ( setting_name = strchr ( tmp_name, '/' ) ) != NULL ) {
		*(setting_name++) = 0;
		settings_name = tmp_name;
	} else {
		setting_name = tmp_name;
		settings_name = NULL;
	}
	if ( ( type_name = strchr ( setting_name, ':' ) ) != NULL )
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
	if ( ( named_setting = find_setting ( setting_name ) ) != NULL ) {
		memcpy ( setting, named_setting, sizeof ( *setting ) );
	} else {
		/* Unrecognised name: try to interpret as a tag number */
		tmp = setting_name;
		while ( 1 ) {
			setting->tag = ( ( setting->tag << 8 ) |
					 strtoul ( tmp, &tmp, 0 ) );
			if ( *tmp == 0 )
				break;
			if ( *tmp != '.' ) {
				DBG ( "Invalid setting \"%s\" in \"%s\"\n",
				      setting_name, name );
				return -ENOENT;
			}
			tmp++;
		}
		setting->tag |= (*settings)->tag_magic;
	}

	/* Identify setting type, if specified */
	if ( type_name ) {
		setting->type = find_setting_type ( type_name );
		if ( setting->type == NULL ) {
			DBG ( "Invalid setting type \"%s\" in \"%s\"\n",
			      type_name, name );
			return -ENOTSUP;
		}
	}

	return 0;
}

/**
 * Parse and store value of named setting
 *
 * @v name		Name of setting
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int storef_named_setting ( const char *name, const char *value ) {
	struct settings *settings;
	struct setting setting;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &setting ) ) != 0 )
		return rc;
	return storef_setting ( settings, &setting, value );
}

/**
 * Fetch and format value of named setting
 *
 * @v name		Name of setting
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
int fetchf_named_setting ( const char *name, char *buf, size_t len ) {
	struct settings *settings;
	struct setting setting;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &setting ) ) != 0 )
		return rc;
	return fetchf_setting ( settings, &setting, buf, len );
}

/******************************************************************************
 *
 * Setting types
 *
 ******************************************************************************
 */

/**
 * Parse and store value of string setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_string ( struct settings *settings, struct setting *setting,
			   const char *value ) {
	return store_setting ( settings, setting, value, strlen ( value ) );
}

/**
 * Fetch and format value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_string ( struct settings *settings, struct setting *setting,
			   char *buf, size_t len ) {
	return fetch_string_setting ( settings, setting, buf, len );
}

/** A string setting type */
struct setting_type setting_type_string __setting_type = {
	.name = "string",
	.storef = storef_string,
	.fetchf = fetchf_string,
};

/**
 * Parse and store value of URI-encoded string setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_uristring ( struct settings *settings,
			      struct setting *setting,
			      const char *value ) {
	char buf[ strlen ( value ) + 1 ]; /* Decoding never expands string */
	size_t len;

	len = uri_decode ( value, buf, sizeof ( buf ) );
	return store_setting ( settings, setting, buf, len );
}

/**
 * Fetch and format value of URI-encoded string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_uristring ( struct settings *settings,
			      struct setting *setting,
			      char *buf, size_t len ) {
	ssize_t raw_len;

	/* We need to always retrieve the full raw string to know the
	 * length of the encoded string.
	 */
	raw_len = fetch_setting ( settings, setting, NULL, 0 );
	if ( raw_len < 0 )
		return raw_len;

	{
		char raw_buf[ raw_len + 1 ];
       
		fetch_string_setting ( settings, setting, raw_buf,
				       sizeof ( raw_buf ) );
		return uri_encode ( raw_buf, buf, len );
	}
}

/** A URI-encoded string setting type */
struct setting_type setting_type_uristring __setting_type = {
	.name = "uristring",
	.storef = storef_uristring,
	.fetchf = fetchf_uristring,
};

/**
 * Parse and store value of IPv4 address setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_ipv4 ( struct settings *settings, struct setting *setting,
			 const char *value ) {
	struct in_addr ipv4;

	if ( inet_aton ( value, &ipv4 ) == 0 )
		return -EINVAL;
	return store_setting ( settings, setting, &ipv4, sizeof ( ipv4 ) );
}

/**
 * Fetch and format value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_ipv4 ( struct settings *settings, struct setting *setting,
			 char *buf, size_t len ) {
	struct in_addr ipv4;
	int raw_len;

	if ( ( raw_len = fetch_ipv4_setting ( settings, setting, &ipv4 ) ) < 0)
		return raw_len;
	return snprintf ( buf, len, "%s", inet_ntoa ( ipv4 ) );
}

/** An IPv4 address setting type */
struct setting_type setting_type_ipv4 __setting_type = {
	.name = "ipv4",
	.storef = storef_ipv4,
	.fetchf = fetchf_ipv4,
};

/**
 * Parse and store value of integer setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int ( struct settings *settings, struct setting *setting,
			const char *value, unsigned int size ) {
	union {
		uint32_t num;
		uint8_t bytes[4];
	} u;
	char *endp;

	u.num = htonl ( strtoul ( value, &endp, 0 ) );
	if ( *endp )
		return -EINVAL;
	return store_setting ( settings, setting, 
			       &u.bytes[ sizeof ( u ) - size ], size );
}

/**
 * Parse and store value of 8-bit integer setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int8 ( struct settings *settings, struct setting *setting,
			 const char *value ) {
	return storef_int ( settings, setting, value, 1 );
}

/**
 * Parse and store value of 16-bit integer setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int16 ( struct settings *settings, struct setting *setting,
			  const char *value ) {
	return storef_int ( settings, setting, value, 2 );
}

/**
 * Parse and store value of 32-bit integer setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int32 ( struct settings *settings, struct setting *setting,
			  const char *value ) {
	return storef_int ( settings, setting, value, 4 );
}

/**
 * Fetch and format value of signed integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_int ( struct settings *settings, struct setting *setting,
			char *buf, size_t len ) {
	long value;
	int rc;

	if ( ( rc = fetch_int_setting ( settings, setting, &value ) ) < 0 )
		return rc;
	return snprintf ( buf, len, "%ld", value );
}

/**
 * Fetch and format value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_uint ( struct settings *settings, struct setting *setting,
			 char *buf, size_t len ) {
	unsigned long value;
	int rc;

	if ( ( rc = fetch_uint_setting ( settings, setting, &value ) ) < 0 )
		return rc;
	return snprintf ( buf, len, "%#lx", value );
}

/** A signed 8-bit integer setting type */
struct setting_type setting_type_int8 __setting_type = {
	.name = "int8",
	.storef = storef_int8,
	.fetchf = fetchf_int,
};

/** A signed 16-bit integer setting type */
struct setting_type setting_type_int16 __setting_type = {
	.name = "int16",
	.storef = storef_int16,
	.fetchf = fetchf_int,
};

/** A signed 32-bit integer setting type */
struct setting_type setting_type_int32 __setting_type = {
	.name = "int32",
	.storef = storef_int32,
	.fetchf = fetchf_int,
};

/** An unsigned 8-bit integer setting type */
struct setting_type setting_type_uint8 __setting_type = {
	.name = "uint8",
	.storef = storef_int8,
	.fetchf = fetchf_uint,
};

/** An unsigned 16-bit integer setting type */
struct setting_type setting_type_uint16 __setting_type = {
	.name = "uint16",
	.storef = storef_int16,
	.fetchf = fetchf_uint,
};

/** An unsigned 32-bit integer setting type */
struct setting_type setting_type_uint32 __setting_type = {
	.name = "uint32",
	.storef = storef_int32,
	.fetchf = fetchf_uint,
};

/**
 * Parse and store value of hex string setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_hex ( struct settings *settings, struct setting *setting,
			const char *value ) {
	char *ptr = ( char * ) value;
	uint8_t bytes[ strlen ( value ) ]; /* cannot exceed strlen(value) */
	unsigned int len = 0;

	while ( 1 ) {
		bytes[len++] = strtoul ( ptr, &ptr, 16 );
		switch ( *ptr ) {
		case '\0' :
			return store_setting ( settings, setting, bytes, len );
		case ':' :
			ptr++;
			break;
		default :
			return -EINVAL;
		}
	}
}

/**
 * Fetch and format value of hex string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_hex ( struct settings *settings, struct setting *setting,
			char *buf, size_t len ) {
	int raw_len;
	int check_len;
	int used = 0;
	int i;

	raw_len = fetch_setting_len ( settings, setting );
	if ( raw_len < 0 )
		return raw_len;

	{
		uint8_t raw[raw_len];

		check_len = fetch_setting ( settings, setting, raw,
					    sizeof ( raw ) );
		if ( check_len < 0 )
			return check_len;
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
	.storef = storef_hex,
	.fetchf = fetchf_hex,
};

/**
 * Parse and store value of UUID setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_uuid ( struct settings *settings __unused,
			 struct setting *setting __unused,
			 const char *value __unused ) {
	return -ENOTSUP;
}

/**
 * Fetch and format value of UUID setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_uuid ( struct settings *settings, struct setting *setting,
			 char *buf, size_t len ) {
	union uuid uuid;
	int raw_len;

	if ( ( raw_len = fetch_uuid_setting ( settings, setting, &uuid ) ) < 0)
		return raw_len;
	return snprintf ( buf, len, "%s", uuid_ntoa ( &uuid ) );
}

/** UUID setting type */
struct setting_type setting_type_uuid __setting_type = {
	.name = "uuid",
	.storef = storef_uuid,
	.fetchf = fetchf_uuid,
};

/******************************************************************************
 *
 * Settings
 *
 ******************************************************************************
 */

/** Hostname setting */
struct setting hostname_setting __setting = {
	.name = "hostname",
	.description = "Host name",
	.tag = DHCP_HOST_NAME,
	.type = &setting_type_string,
};

/** Filename setting */
struct setting filename_setting __setting = {
	.name = "filename",
	.description = "Boot filename",
	.tag = DHCP_BOOTFILE_NAME,
	.type = &setting_type_string,
};

/** Root path setting */
struct setting root_path_setting __setting = {
	.name = "root-path",
	.description = "NFS/iSCSI root path",
	.tag = DHCP_ROOT_PATH,
	.type = &setting_type_string,
};

/** Username setting */
struct setting username_setting __setting = {
	.name = "username",
	.description = "User name",
	.tag = DHCP_EB_USERNAME,
	.type = &setting_type_string,
};

/** Password setting */
struct setting password_setting __setting = {
	.name = "password",
	.description = "Password",
	.tag = DHCP_EB_PASSWORD,
	.type = &setting_type_string,
};

/** Priority setting */
struct setting priority_setting __setting = {
	.name = "priority",
	.description = "Priority of these settings",
	.tag = DHCP_EB_PRIORITY,
	.type = &setting_type_int8,
};
