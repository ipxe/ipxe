#ifndef _GPXE_EDITSTRING_H
#define _GPXE_EDITSTRING_H

/** @file
 *
 * Editable strings
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** An editable string */
struct edit_string {
	/** Buffer for string */
	char *buf;
	/** Size of buffer (including terminating NUL) */
	size_t len;
	/** Cursor position */
	unsigned int cursor;

	/* The following items are the edit history */

	/** Last cursor position */
	unsigned int last_cursor;
	/** Start of modified portion of string */
	unsigned int mod_start;
	/** End of modified portion of string */
	unsigned int mod_end;
};

extern int edit_string ( struct edit_string *string, int key ) __nonnull;

#endif /* _GPXE_EDITSTRING_H */
