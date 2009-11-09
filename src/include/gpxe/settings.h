#ifndef _GPXE_SETTINGS_H
#define _GPXE_SETTINGS_H

/** @file
 *
 * Configuration settings
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/tables.h>
#include <gpxe/list.h>
#include <gpxe/refcnt.h>

struct settings;
struct in_addr;
union uuid;

/** A setting */
struct setting {
	/** Name
	 *
	 * This is the human-readable name for the setting.
	 */
	const char *name;
	/** Description */
	const char *description;
	/** Setting type
	 *
	 * This identifies the type of setting (e.g. string, IPv4
	 * address, etc.).
	 */
	struct setting_type *type;
	/** DHCP option number, if applicable */
	unsigned int tag;
};

/** Configuration setting table */
#define SETTINGS __table ( struct setting, "settings" )

/** Declare a configuration setting */
#define __setting __table_entry ( SETTINGS, 01 )

/** Settings block operations */
struct settings_operations {
	/** Store value of setting
	 *
	 * @v settings		Settings block
	 * @v setting		Setting to store
	 * @v data		Setting data, or NULL to clear setting
	 * @v len		Length of setting data
	 * @ret rc		Return status code
	 */
	int ( * store ) ( struct settings *settings, struct setting *setting,
			  const void *data, size_t len );
	/** Fetch value of setting
	 *
	 * @v settings		Settings block
	 * @v setting		Setting to fetch
	 * @v data		Buffer to fill with setting data
	 * @v len		Length of buffer
	 * @ret len		Length of setting data, or negative error
	 *
	 * The actual length of the setting will be returned even if
	 * the buffer was too small.
	 */
	int ( * fetch ) ( struct settings *settings, struct setting *setting,
			  void *data, size_t len );
	/** Clear settings block
	 *
	 * @v settings		Settings block
	 */
	void ( * clear ) ( struct settings *settings );
};

/** A settings block */
struct settings {
	/** Reference counter */
	struct refcnt *refcnt;
	/** Name */
	const char *name;
	/** Tag magic
	 *
	 * This value will be ORed in to any numerical tags
	 * constructed by parse_setting_name(), and can be used to
	 * avoid e.g. attempting to retrieve the subnet mask from
	 * SMBIOS, or the system UUID from DHCP.
	 */
	unsigned int tag_magic;
	/** Parent settings block */
	struct settings *parent;
	/** Sibling settings blocks */
	struct list_head siblings;
	/** Child settings blocks */
	struct list_head children;
	/** Settings block operations */
	struct settings_operations *op;
};

/**
 * A setting type
 *
 * This represents a type of setting (e.g. string, IPv4 address,
 * etc.).
 */
struct setting_type {
	/** Name
	 *
	 * This is the name exposed to the user (e.g. "string").
	 */
	const char *name;
	/** Parse and set value of setting
	 *
	 * @v settings		Settings block
	 * @v setting		Setting to store
	 * @v value		Formatted setting data
	 * @ret rc		Return status code
	 */
	int ( * storef ) ( struct settings *settings, struct setting *setting,
			   const char *value );
	/** Fetch and format value of setting
	 *
	 * @v settings		Settings block
	 * @v setting		Setting to fetch
	 * @v buf		Buffer to contain formatted value
	 * @v len		Length of buffer
	 * @ret len		Length of formatted value, or negative error
	 */
	int ( * fetchf ) ( struct settings *settings, struct setting *setting,
			   char *buf, size_t len );
};

/** Configuration setting type table */
#define SETTING_TYPES __table ( struct setting_type, "setting_types" )

/** Declare a configuration setting type */
#define __setting_type __table_entry ( SETTING_TYPES, 01 )

/**
 * A settings applicator
 *
 */
struct settings_applicator {
	/** Apply updated settings
	 *
	 * @ret rc		Return status code
	 */
	int ( * apply ) ( void );
};

/** Settings applicator table */
#define SETTINGS_APPLICATORS \
	__table ( struct settings_applicator, "settings_applicators" )

/** Declare a settings applicator */
#define __settings_applicator __table_entry ( SETTINGS_APPLICATORS, 01 )

/**
 * A generic settings block
 *
 */
struct generic_settings {
	/** Settings block */
	struct settings settings;
	/** List of generic settings */
	struct list_head list;
};

extern struct settings_operations generic_settings_operations;
extern int generic_settings_store ( struct settings *settings,
				    struct setting *setting,
				    const void *data, size_t len );
extern int generic_settings_fetch ( struct settings *settings,
				    struct setting *setting,
				    void *data, size_t len );
extern void generic_settings_clear ( struct settings *settings );

extern int register_settings ( struct settings *settings,
			       struct settings *parent );
extern void unregister_settings ( struct settings *settings );

extern int store_setting ( struct settings *settings, struct setting *setting,
			   const void *data, size_t len );
extern int fetch_setting ( struct settings *settings, struct setting *setting,
			   void *data, size_t len );
extern int fetch_setting_len ( struct settings *settings,
			       struct setting *setting );
extern int fetch_string_setting ( struct settings *settings,
				  struct setting *setting,
				  char *data, size_t len );
extern int fetch_string_setting_copy ( struct settings *settings,
				       struct setting *setting,
				       char **data );
extern int fetch_ipv4_setting ( struct settings *settings,
				struct setting *setting, struct in_addr *inp );
extern int fetch_int_setting ( struct settings *settings,
			       struct setting *setting, long *value );
extern int fetch_uint_setting ( struct settings *settings,
				struct setting *setting,
				unsigned long *value );
extern long fetch_intz_setting ( struct settings *settings,
				 struct setting *setting );
extern unsigned long fetch_uintz_setting ( struct settings *settings,
					   struct setting *setting );
extern int fetch_uuid_setting ( struct settings *settings,
				struct setting *setting, union uuid *uuid );
extern void clear_settings ( struct settings *settings );
extern int setting_cmp ( struct setting *a, struct setting *b );

extern struct settings * find_settings ( const char *name );

extern int storef_setting ( struct settings *settings,
			    struct setting *setting,
			    const char *value );
extern int storef_named_setting ( const char *name, const char *value );
extern int fetchf_named_setting ( const char *name, char *buf, size_t len );

extern struct setting_type setting_type_string __setting_type;
extern struct setting_type setting_type_ipv4 __setting_type;
extern struct setting_type setting_type_int8 __setting_type;
extern struct setting_type setting_type_int16 __setting_type;
extern struct setting_type setting_type_int32 __setting_type;
extern struct setting_type setting_type_uint8 __setting_type;
extern struct setting_type setting_type_uint16 __setting_type;
extern struct setting_type setting_type_uint32 __setting_type;
extern struct setting_type setting_type_hex __setting_type;
extern struct setting_type setting_type_uuid __setting_type;

extern struct setting ip_setting __setting;
extern struct setting netmask_setting __setting;
extern struct setting gateway_setting __setting;
extern struct setting dns_setting __setting;
extern struct setting domain_setting __setting;
extern struct setting hostname_setting __setting;
extern struct setting filename_setting __setting;
extern struct setting root_path_setting __setting;
extern struct setting username_setting __setting;
extern struct setting password_setting __setting;
extern struct setting priority_setting __setting;
extern struct setting uuid_setting __setting;
extern struct setting next_server_setting __setting;
extern struct setting mac_setting __setting;
extern struct setting busid_setting __setting;
extern struct setting user_class_setting __setting;

/**
 * Initialise a settings block
 *
 * @v settings		Settings block
 * @v op		Settings block operations
 * @v refcnt		Containing object reference counter, or NULL
 * @v name		Settings block name
 * @v tag_magic		Tag magic
 */
static inline void settings_init ( struct settings *settings,
				   struct settings_operations *op,
				   struct refcnt *refcnt,
				   const char *name,
				   unsigned int tag_magic ) {
	INIT_LIST_HEAD ( &settings->siblings );
	INIT_LIST_HEAD ( &settings->children );
	settings->op = op;
	settings->refcnt = refcnt;
	settings->name = name;
	settings->tag_magic = tag_magic;
}

/**
 * Initialise a settings block
 *
 * @v generics		Generic settings block
 * @v refcnt		Containing object reference counter, or NULL
 * @v name		Settings block name
 */
static inline void generic_settings_init ( struct generic_settings *generics,
					   struct refcnt *refcnt,
					   const char *name ) {
	settings_init ( &generics->settings, &generic_settings_operations,
			refcnt, name, 0 );
	INIT_LIST_HEAD ( &generics->list );
}

/**
 * Delete setting
 *
 * @v settings		Settings block
 * @v setting		Setting to delete
 * @ret rc		Return status code
 */
static inline int delete_setting ( struct settings *settings,
				   struct setting *setting ) {
	return store_setting ( settings, setting, NULL, 0 );
}

/**
 * Fetch and format value of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v type		Settings type
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static inline int fetchf_setting ( struct settings *settings,
				   struct setting *setting,
				   char *buf, size_t len ) {
	return setting->type->fetchf ( settings, setting, buf, len );
}

/**
 * Delete named setting
 *
 * @v name		Name of setting
 * @ret rc		Return status code
 */
static inline int delete_named_setting ( const char *name ) {
	return storef_named_setting ( name, NULL );
}

/**
 * Check existence of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @ret exists		Setting exists
 */
static inline int setting_exists ( struct settings *settings,
				   struct setting *setting ) {
	return ( fetch_setting_len ( settings, setting ) >= 0 );
}

#endif /* _GPXE_SETTINGS_H */
