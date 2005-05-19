#ifndef VSPRINTF_H
#define VSPRINTF_H

/** @file
 *
 * printf and friends.
 *
 * Etherboot's printf() functions understand the following format
 * specifiers:
 *
 *	- Hexadecimal integers
 *		- @c %[#]x	- 4 bytes int (8 hex digits, lower case)
 *		- @c %[#]X	- 4 bytes int (8 hex digits, upper case)
 *		- @c %[#]lx	- 8 bytes long (16 hex digits, lower case)
 *		- @c %[#]lX	- 8 bytes long (16 hex digits, upper case)
 *		- @c %[#]hx	- 2 bytes int (4 hex digits, lower case)
 *		- @c %[#]hX	- 2 bytes int (4 hex digits, upper case)
 *		- @c %[#]hhx	- 1 byte int (2 hex digits, lower case)
 *		- @c %[#]hhX	- 1 byte int (2 hex digits, upper case)
 *		.
 *		If the optional # prefix is specified, the output will
 *		be prefixed with 0x (or 0X).
 *
 *	- Other integers
 *		- @c %d		- decimal int
 *	.
 *	Note that any width specification (e.g. the @c 02 in @c %02x)
 *	will be accepted but ignored.
 *
 *	- Strings and characters
 *		- @c %c		- char
 *		- @c %s		- string
 *		- @c %m		- error message text (i.e. strerror(errno))
 *
 *	- Etherboot-specific specifiers
 *		- @c %@		- IP address in ddd.ddd.ddd.ddd notation
 *		- @c %!		- MAC address in xx:xx:xx:xx:xx:xx notation
 *
 *
 * @note Unfortunately, we cannot use <tt> __attribute__ (( format (
 * printf, ... ) )) </tt> to get automatic type checking on arguments,
 * because we use non-standard format characters such as @c %! and
 * @c %@.
 *
 */

extern int sprintf ( char *buf, const char *fmt, ... );
extern void printf ( const char *fmt, ... );

#endif /* VSPRINTF_H */
