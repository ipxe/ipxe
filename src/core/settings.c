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

/** Registered settings applicators */
static struct settings_applicator settings_applicators[0]
	__table_start ( struct settings_applicator, settings_applicators );
static struct settings_applicator settings_applicators_end[0]
	__table_end ( struct settings_applicator, settings_applicators );

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

// Dummy routine just for testing
int simple_settings_store ( struct settings *settings, unsigned int tag,
			    const void *data, size_t len ) {
	DBGC ( settings, "Settings %p: store %s to:\n",
	       settings, setting_tag_name ( tag ) );
	DBGC_HD ( settings, data, len );
	return 0;
}

// Dummy routine just for testing
int simple_settings_fetch ( struct settings *settings, unsigned int tag,
			    void *data, size_t len ) {
	( void ) settings;
	( void ) tag;
	( void ) data;
	( void ) len;
	return -ENOENT;
}

/** Simple settings operations */
struct settings_operations simple_settings_operations = {
	.store = simple_settings_store,
	.fetch = simple_settings_fetch,
};

/** Root settings block */
struct settings settings_root = {
	.refcnt = NULL,
	.name = "",
	.siblings = LIST_HEAD_INIT ( settings_root.siblings ),
	.children = LIST_HEAD_INIT ( settings_root.children ),
	.op = &simple_settings_operations,
};

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
	priority = fetch_intz_setting ( settings, DHCP_EB_PRIORITY );

	/* Remove from siblings list */
	list_del ( &settings->siblings );

	/* Reinsert after any existing blocks which have a higher priority */
	list_for_each_entry ( tmp, &parent->children, siblings ) {
		tmp_priority = fetch_intz_setting ( tmp, DHCP_EB_PRIORITY );
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

	/* NULL parent => add to settings root */
	assert ( settings != NULL );
	if ( parent == NULL )
		parent = &settings_root;

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
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
int store_setting ( struct settings *settings, unsigned int tag,
		    const void *data, size_t len ) {
	struct settings *parent;
	int rc;

	/* Sanity check */
	if ( ! settings )
		return -ENODEV;

	/* Store setting */
	if ( ( rc = settings->op->store ( settings, tag, data, len ) ) != 0 )
		return rc;

	/* Reprioritise settings if necessary */
	if ( tag == DHCP_EB_PRIORITY )
		reprioritise_settings ( settings );

	/* If these settings are registered, apply potentially-updated
	 * settings
	 */
	for ( parent = settings->parent ; parent ; parent = parent->parent ) {
		if ( parent == &settings_root ) {
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
 * @v tag		Setting tag number
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 *
 * The actual length of the setting will be returned even if
 * the buffer was too small.
 */
int fetch_setting ( struct settings *settings, unsigned int tag,
		    void *data, size_t len ) {
	struct settings *child;
	int ret;

	/* NULL settings implies starting at the global settings root */
	if ( ! settings )
		settings = &settings_root;

	/* Try this block first */
	if ( ( ret = settings->op->fetch ( settings, tag, data, len ) ) >= 0)
		return ret;

	/* Recurse into each child block in turn */
	list_for_each_entry ( child, &settings->children, siblings ) {
		if ( ( ret = fetch_setting ( child, tag, data, len ) ) >= 0)
			return ret;
	}

	return -ENOENT;
}

/**
 * Fetch length of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @ret len		Length of setting data, or negative error
 *
 * This function can also be used as an existence check for the
 * setting.
 */
int fetch_setting_len ( struct settings *settings, unsigned int tag ) {
	return fetch_setting ( settings, tag, NULL, 0 );
}

/**
 * Fetch value of string setting
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
int fetch_string_setting ( struct settings *settings, unsigned int tag,
			   char *data, size_t len ) {
	memset ( data, 0, len );
	return fetch_setting ( settings, tag, data, ( len - 1 ) );
}

/**
 * Fetch value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v inp		IPv4 address to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_ipv4_setting ( struct settings *settings, unsigned int tag,
			 struct in_addr *inp ) {
	int len;

	len = fetch_setting ( settings, tag, inp, sizeof ( *inp ) );
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
 * @v tag		Setting tag number
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_int_setting ( struct settings *settings, unsigned int tag,
			long *value ) {
	union {
		long value;
		uint8_t u8[ sizeof ( long ) ];
		int8_t s8[ sizeof ( long ) ];
	} buf;
	int len;
	int i;

	buf.value = 0;
	len = fetch_setting ( settings, tag, &buf, sizeof ( buf ) );
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
 * Fetch value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v value		Integer value to fill in
 * @ret len		Length of setting, or negative error
 */
int fetch_uint_setting ( struct settings *settings, unsigned int tag,
			 unsigned long *value ) {
	long svalue;
	int len;

	len = fetch_int_setting ( settings, tag, &svalue );
	if ( len < 0 )
		return len;

	*value = ( svalue & ( -1UL >> ( sizeof ( long ) - len ) ) );

	return len;
}

/**
 * Fetch value of signed integer setting, or zero
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @ret value		Setting value, or zero
 */
long fetch_intz_setting ( struct settings *settings, unsigned int tag ) {
	long value = 0;

	fetch_int_setting ( settings, tag, &value );
	return value;
}

/**
 * Fetch value of unsigned integer setting, or zero
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @ret value		Setting value, or zero
 */
unsigned long fetch_uintz_setting ( struct settings *settings,
				    unsigned int tag ) {
	unsigned long value = 0;

	fetch_uint_setting ( settings, tag, &value );
	return value;
}

/**
 * Copy setting
 *
 * @v dest		Destination settings block
 * @v dest_tag		Destination setting tag number
 * @v source		Source settings block
 * @v source_tag	Source setting tag number
 * @ret rc		Return status code
 */
int copy_setting ( struct settings *dest, unsigned int dest_tag,
		   struct settings *source, unsigned int source_tag ) {
	int len;
	int check_len;
	int rc;

	len = fetch_setting_len ( source, source_tag );
	if ( len < 0 )
		return len;

	{
		char buf[len];

		check_len = fetch_setting ( source, source_tag, buf,
					    sizeof ( buf ) );
		assert ( check_len == len );

		if ( ( rc = store_setting ( dest, dest_tag, buf,
					    sizeof ( buf ) ) ) != 0 )
			return rc;
	}

	return 0;
}

/**
 * Copy settings
 *
 * @v dest		Destination settings block
 * @v source		Source settings block
 * @v encapsulator	Encapsulating setting tag number, or zero
 * @ret rc		Return status code
 */
static int copy_encap_settings ( struct settings *dest,
				 struct settings *source,
				 unsigned int encapsulator ) {
	unsigned int subtag;
	unsigned int tag;
	int rc;

	for ( subtag = DHCP_MIN_OPTION; subtag <= DHCP_MAX_OPTION; subtag++ ) {
		tag = DHCP_ENCAP_OPT ( encapsulator, subtag );
		switch ( tag ) {
		case DHCP_EB_ENCAP:
		case DHCP_VENDOR_ENCAP:
			/* Process encapsulated options field */
			if ( ( rc = copy_encap_settings ( dest, source,
							  tag ) ) != 0 )
				return rc;
			break;
		default:
			/* Copy option to reassembled packet */
			if ( ( rc = copy_setting ( dest, tag, source,
						   tag ) ) != 0 )
				return rc;
			break;
		}
	}

	return 0;
}

/**
 * Copy settings
 *
 * @v dest		Destination settings block
 * @v source		Source settings block
 * @ret rc		Return status code
 */
int copy_settings ( struct settings *dest, struct settings *source ) {
	return copy_encap_settings ( dest, source, 0 );
}

/******************************************************************************
 *
 * Named and typed setting routines
 *
 ******************************************************************************
 */

/**
 * Store value of typed setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v type		Settings type
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int store_typed_setting ( struct settings *settings,
			  unsigned int tag, struct setting_type *type,
			  const char *value ) {

	/* NULL value implies deletion.  Avoid imposing the burden of
	 * checking for NULL values on each typed setting's storef()
	 * method.
	 */
	if ( ! value )
		return delete_setting ( settings, tag );
		
	return type->storef ( settings, tag, value );
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
	*settings = &settings_root;
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
 * Parse and store value of named setting
 *
 * @v name		Name of setting
 * @v value		Formatted setting data, or NULL
 * @ret rc		Return status code
 */
int store_named_setting ( const char *name, const char *value ) {
	struct settings *settings;
	unsigned int tag;
	struct setting_type *type;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &tag,
					 &type ) ) != 0 )
		return rc;
	return store_typed_setting ( settings, tag, type, value );
}

/**
 * Fetch and format value of named setting
 *
 * @v name		Name of setting
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
int fetch_named_setting ( const char *name, char *buf, size_t len ) {
	struct settings *settings;
	unsigned int tag;
	struct setting_type *type;
	int rc;

	if ( ( rc = parse_setting_name ( name, &settings, &tag,
					 &type ) ) != 0 )
		return rc;
	return fetch_typed_setting ( settings, tag, type, buf, len );
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
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_string ( struct settings *settings, unsigned int tag,
			   const char *value ) {
	return store_setting ( settings, tag, value, strlen ( value ) );
}

/**
 * Fetch and format value of string setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_string ( struct settings *settings, unsigned int tag,
			   char *buf, size_t len ) {
	return fetch_string_setting ( settings, tag, buf, len );
}

/** A string setting type */
struct setting_type setting_type_string __setting_type = {
	.name = "string",
	.storef = storef_string,
	.fetchf = fetchf_string,
};

/**
 * Parse and store value of IPv4 address setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_ipv4 ( struct settings *settings, unsigned int tag,
			 const char *value ) {
	struct in_addr ipv4;

	if ( inet_aton ( value, &ipv4 ) == 0 )
		return -EINVAL;
	return store_setting ( settings, tag, &ipv4, sizeof ( ipv4 ) );
}

/**
 * Fetch and format value of IPv4 address setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_ipv4 ( struct settings *settings, unsigned int tag,
			 char *buf, size_t len ) {
	struct in_addr ipv4;
	int rc;

	if ( ( rc = fetch_ipv4_setting ( settings, tag, &ipv4 ) ) < 0 )
		return rc;
	return snprintf ( buf, len, inet_ntoa ( ipv4 ) );
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
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int ( struct settings *settings, unsigned int tag,
			const char *value, unsigned int size ) {
	union {
		uint32_t num;
		uint8_t bytes[4];
	} u;
	char *endp;

	u.num = htonl ( strtoul ( value, &endp, 0 ) );
	if ( *endp )
		return -EINVAL;
	return store_setting ( settings, tag, 
			       &u.bytes[ sizeof ( u ) - size ], size );
}

/**
 * Parse and store value of 8-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int8 ( struct settings *settings, unsigned int tag,
			 const char *value ) {
	return storef_int ( settings, tag, value, 1 );
}

/**
 * Parse and store value of 16-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int16 ( struct settings *settings, unsigned int tag,
			  const char *value ) {
	return storef_int ( settings, tag, value, 2 );
}

/**
 * Parse and store value of 32-bit integer setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @v size		Integer size, in bytes
 * @ret rc		Return status code
 */
static int storef_int32 ( struct settings *settings, unsigned int tag,
			  const char *value ) {
	return storef_int ( settings, tag, value, 4 );
}

/**
 * Fetch and format value of signed integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_int ( struct settings *settings, unsigned int tag,
			char *buf, size_t len ) {
	long value;
	int rc;

	if ( ( rc = fetch_int_setting ( settings, tag, &value ) ) < 0 )
		return rc;
	return snprintf ( buf, len, "%ld", value );
}

/**
 * Fetch and format value of unsigned integer setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_uint ( struct settings *settings, unsigned int tag,
			 char *buf, size_t len ) {
	unsigned long value;
	int rc;

	if ( ( rc = fetch_uint_setting ( settings, tag, &value ) ) < 0 )
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
 * @v tag		Setting tag number
 * @v value		Formatted setting data
 * @ret rc		Return status code
 */
static int storef_hex ( struct settings *settings, unsigned int tag,
			const char *value ) {
	char *ptr = ( char * ) value;
	uint8_t bytes[ strlen ( value ) ]; /* cannot exceed strlen(value) */
	unsigned int len = 0;

	while ( 1 ) {
		bytes[len++] = strtoul ( ptr, &ptr, 16 );
		switch ( *ptr ) {
		case '\0' :
			return store_setting ( settings, tag, bytes, len );
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
 * @v tag		Setting tag number
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static int fetchf_hex ( struct settings *settings, unsigned int tag,
			char *buf, size_t len ) {
	int raw_len;
	int check_len;
	int used = 0;
	int i;

	raw_len = fetch_setting_len ( settings, tag );
	if ( raw_len < 0 )
		return raw_len;

	{
		uint8_t raw[raw_len];

		check_len = fetch_setting ( settings, tag, raw, sizeof (raw) );
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
		.description = "IPv4 address",
		.tag = DHCP_EB_YIADDR,
		.type = &setting_type_ipv4,
	},
	{
		.name = "netmask",
		.description = "IPv4 subnet mask",
		.tag = DHCP_SUBNET_MASK,
		.type = &setting_type_ipv4,
	},
	{
		.name = "gateway",
		.description = "Default gateway",
		.tag = DHCP_ROUTERS,
		.type = &setting_type_ipv4,
	},
	{
		.name = "dns",
		.description = "DNS server",
		.tag = DHCP_DNS_SERVERS,
		.type = &setting_type_ipv4,
	},
	{
		.name = "hostname",
		.description = "Host name",
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
		.description = "User name",
		.tag = DHCP_EB_USERNAME,
		.type = &setting_type_string,
	},
	{
		.name = "password",
		.description = "Password",
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
		.description = "Priority of these settings",
		.tag = DHCP_EB_PRIORITY,
		.type = &setting_type_int8,
	},
};
