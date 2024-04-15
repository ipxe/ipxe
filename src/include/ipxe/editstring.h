#ifndef _IPXE_EDITSTRING_H
#define _IPXE_EDITSTRING_H

/** @file
 *
 * Editable strings
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** An editable string */
struct edit_string {
	/** Dynamically allocated string buffer */
	char **buf;
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

/**
 * Initialise editable string
 *
 * @v string		Editable string
 * @v buf		Dynamically allocated string buffer
 *
 * The @c buf parameter must be the address of a caller-provided
 * pointer to a NUL-terminated string allocated using malloc() (or
 * equivalent, such as strdup()).  Any edits made to the string will
 * realloc() the string buffer as needed.
 *
 * The caller may choose leave the initial string buffer pointer as @c
 * NULL, in which case it will be allocated upon the first attempt to
 * insert a character into the buffer.  If the caller does this, then
 * it must be prepared to find the pointer still @c NULL after
 * editing, since the user may never attempt to insert any characters.
 */
static inline __nonnull void init_editstring ( struct edit_string *string,
					       char **buf ) {

	string->buf = buf;
}

extern __attribute__ (( nonnull ( 1 ) )) int
replace_string ( struct edit_string *string, const char *replacement );

extern __nonnull int edit_string ( struct edit_string *string, int key );

#endif /* _IPXE_EDITSTRING_H */
