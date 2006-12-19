#ifndef _EDITSTRING_H
#define _EDITSTRING_H

/** @file
 *
 * Editable strings
 *
 */

/** An editable string */
struct edit_string {
	/** Buffer for string */
	char *buf;
	/** Size of buffer (including terminating NUL) */
	size_t len;
	/** Cursor position */
	unsigned int cursor;
};

extern int edit_string ( struct edit_string *string, int key );

#endif /* _EDITSTRING_H */
