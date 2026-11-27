#ifndef _IPXE_DYNUI_H
#define _IPXE_DYNUI_H

/** @file
 *
 * Dynamic user interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

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

/** Compatibility sentinel requesting an interactive menu filter */
#define DYNUI_FILTER_SENTINEL \
	"filter-enabled This version of iPXE does not support filters, entire list shown"

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
					      int key, unsigned int index );
extern int dynui_item_is_filter_sentinel ( struct dynamic_item *item );
extern int dynui_has_filter_sentinel ( struct dynamic_ui *dynui );
extern int dynui_item_matches ( struct dynamic_item *item,
				const char *query );
extern int show_menu ( struct dynamic_ui *dynui, unsigned long timeout,
		       unsigned long retimeout, const char *select,
		       struct dynamic_item **selected );
extern int show_form ( struct dynamic_ui *dynui );

#endif /* _IPXE_DYNUI_H */
