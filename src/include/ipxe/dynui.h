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
	/** Number of user interface items */
	unsigned int count;
};

/** A dynamic user interface item */
struct dynamic_item {
	/** List of dynamic user interface items */
	struct list_head list;
	/** Name */
	const char *name;
	/** Text */
	const char *text;
	/** Index */
	unsigned int index;
	/** Flags */
	unsigned int flags;
	/** Shortcut key */
	int shortcut;
};

/** Dynamic user interface item is default selection */
#define DYNUI_DEFAULT 0x0001

/** Dynamic user interface item represents a secret */
#define DYNUI_SECRET 0x0002

extern struct dynamic_ui * create_dynui ( const char *name, const char *title );
extern struct dynamic_item * add_dynui_item ( struct dynamic_ui *dynui,
					      const char *name,
					      const char *text,
					      unsigned int flags,
					      int shortcut );
extern void destroy_dynui ( struct dynamic_ui *dynui );
extern struct dynamic_ui * find_dynui ( const char *name );
extern struct dynamic_item * dynui_item ( struct dynamic_ui *dynui,
					  unsigned int index );
extern struct dynamic_item * dynui_shortcut ( struct dynamic_ui *dynui,
					      int key );
extern int show_menu ( struct dynamic_ui *dynui, unsigned long timeout,
		       unsigned long retimeout, const char *select,
		       struct dynamic_item **selected );
extern int show_form ( struct dynamic_ui *dynui );

#endif /* _IPXE_DYNUI_H */
