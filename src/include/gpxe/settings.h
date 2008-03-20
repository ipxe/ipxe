#ifndef _GPXE_SETTINGS_H
#define _GPXE_SETTINGS_H

/** @file
 *
 * Configuration settings
 *
 */

#include <stdint.h>
#include <gpxe/tables.h>
#include <gpxe/list.h>
#include <gpxe/refcnt.h>

struct settings;
struct in_addr;

/** Settings block operations */
struct settings_operations {
	/** Store value of setting
	 *
	 * @v settings		Settings block
	 * @v tag		Setting tag number
	 * @v data		Setting data, or NULL to clear setting
	 * @v len		Length of setting data
	 * @ret rc		Return status code
	 */
	int ( * store ) ( struct settings *settings, unsigned int tag,
			  const void *data, size_t len );
	/** Fetch value of setting
	 *
	 * @v settings		Settings block
	 * @v tag		Setting tag number
	 * @v data		Buffer to fill with setting data
	 * @v len		Length of buffer
	 * @ret len		Length of setting data, or negative error
	 *
	 * The actual length of the setting will be returned even if
	 * the buffer was too small.
	 */
	int ( * fetch ) ( struct settings *settings, unsigned int tag,
			  void *data, size_t len );
};

/** A settings block */
struct settings {
	/** Reference counter */
	struct refcnt *refcnt;
	/** Name */
	const char *name;
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
	 * @v tag		Setting tag number
	 * @v value		Formatted setting data
	 * @ret rc		Return status code
	 */
	int ( * storef ) ( struct settings *settings, unsigned int tag,
			   const char *value );
	/** Fetch and format value of setting
	 *
	 * @v settings		Settings block, or NULL to search all blocks
	 * @v tag		Setting tag number
	 * @v buf		Buffer to contain formatted value
	 * @v len		Length of buffer
	 * @ret len		Length of formatted value, or negative error
	 */
	int ( * fetchf ) ( struct settings *settings, unsigned int tag,
			   char *buf, size_t len );
};

/** Declare a configuration setting type */
#define	__setting_type \
	__table ( struct setting_type, setting_types, 01 )

/**
 * A named setting
 *
 * This represents a single setting (e.g. "hostname"), encapsulating
 * the information about the setting's tag number and type.
 */
struct named_setting {
	/** Name
	 *
	 * This is the human-readable name for the setting.
	 */
	const char *name;
	/** Description */
	const char *description;
	/** Setting tag number */
	unsigned int tag;
	/** Setting type
	 *
	 * This identifies the type of setting (e.g. string, IPv4
	 * address, etc.).
	 */
	struct setting_type *type;
};

/** Declare a configuration setting */
#define	__named_setting __table ( struct named_setting, named_settings, 01 )

extern int simple_settings_store ( struct settings *settings, unsigned int tag,
				   const void *data, size_t len );
extern int simple_settings_fetch ( struct settings *settings, unsigned int tag,
				   void *data, size_t len );
extern struct settings_operations simple_settings_operations;

extern int register_settings ( struct settings *settings,
			       struct settings *parent );
extern void unregister_settings ( struct settings *settings );
extern int fetch_setting ( struct settings *settings, unsigned int tag,
			   void *data, size_t len );
extern int fetch_setting_len ( struct settings *settings, unsigned int tag );
extern int fetch_string_setting ( struct settings *settings, unsigned int tag,
				  char *data, size_t len );
extern int fetch_ipv4_setting ( struct settings *settings, unsigned int tag,
				struct in_addr *inp );
extern int fetch_int_setting ( struct settings *settings, unsigned int tag,
			       long *value );
extern int fetch_uint_setting ( struct settings *settings, unsigned int tag,
				unsigned long *value );
extern struct settings * find_settings ( const char *name );
extern int store_typed_setting ( struct settings *settings,
				 unsigned int tag, struct setting_type *type,
				 const char *value );
extern int store_named_setting ( const char *name, const char *value );
extern int fetch_named_setting ( const char *name, char *buf, size_t len );

extern struct setting_type setting_type_ __setting_type;
extern struct setting_type setting_type_string __setting_type;
extern struct setting_type setting_type_ipv4 __setting_type;
extern struct setting_type setting_type_int8 __setting_type;
extern struct setting_type setting_type_int16 __setting_type;
extern struct setting_type setting_type_int32 __setting_type;
extern struct setting_type setting_type_uint8 __setting_type;
extern struct setting_type setting_type_uint16 __setting_type;
extern struct setting_type setting_type_uint32 __setting_type;
extern struct setting_type setting_type_hex __setting_type;

/**
 * Initialise a settings block
 *
 * @v settings		Settings block
 * @v op		Settings block operations
 * @v refcnt		Containing object reference counter, or NULL
 * @v name		Settings block name
 */
static inline void settings_init ( struct settings *settings,
				   struct settings_operations *op,
				   struct refcnt *refcnt,
				   const char *name ) {
	INIT_LIST_HEAD ( &settings->siblings );
	INIT_LIST_HEAD ( &settings->children );
	settings->op = op;
	settings->refcnt = refcnt;
	settings->name = name;
}

/**
 * Store value of setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static inline int store_setting ( struct settings *settings, unsigned int tag,
				  const void *data, size_t len ) {
	return settings->op->store ( settings, tag, data, len );
}

/**
 * Delete setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @ret rc		Return status code
 */
static inline int delete_setting ( struct settings *settings,
				   unsigned int tag ) {
	return store_setting ( settings, tag, NULL, 0 );
}

/**
 * Fetch and format value of setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v tag		Setting tag number
 * @v type		Settings type
 * @v buf		Buffer to contain formatted value
 * @v len		Length of buffer
 * @ret len		Length of formatted value, or negative error
 */
static inline int fetch_typed_setting ( struct settings *settings,
					unsigned int tag,
					struct setting_type *type,
					char *buf, size_t len ) {
	return type->fetchf ( settings, tag, buf, len );
}

/**
 * Delete named setting
 *
 * @v name		Name of setting
 * @ret rc		Return status code
 */
static inline int delete_named_setting ( const char *name ) {
	return store_named_setting ( name, NULL );
}

#endif /* _GPXE_SETTINGS_H */
