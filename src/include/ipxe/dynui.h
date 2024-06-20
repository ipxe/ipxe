#ifndef _IPXE_DYNUI_H
#define _IPXE_DYNUI_H

/** @file
 *
 * Dynamic user interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/list.h>

/** A dynamic user interface */
struct dynamic_ui {
	/** List of dynamic user interfaces */
	struct list_head list;
	/** Name */
	const char *name;
	/** Title */
	const char *title;
	/** Dynamic user interface items */
	struct list_head items;
};

/** A dynamic user interface item */
struct dynamic_item {
	/** List of dynamic user interface items */
	struct list_head list;
	/** Name */
	const char *name;
	/** Text */
	const char *text;
	/** Shortcut key */
	int shortcut;
	/** Is default item */
	int is_default;
};

extern struct dynamic_ui * create_dynui ( const char *name, const char *title );
extern struct dynamic_item * add_dynui_item ( struct dynamic_ui *dynui,
					      const char *name,
					      const char *text, int shortcut,
					      int is_default );
extern void destroy_dynui ( struct dynamic_ui *dynui );
extern struct dynamic_ui * find_dynui ( const char *name );
extern int show_menu ( struct dynamic_ui *dynui, unsigned long timeout,
		       const char *select, struct dynamic_item **selected );

#endif /* _IPXE_DYNUI_H */
