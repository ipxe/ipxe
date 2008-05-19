#ifndef _CTYPE_H
#define _CTYPE_H

/** @file
 *
 * Character types
 */

#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define islower(c)	((c) >= 'a' && (c) <= 'z')
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')

static inline unsigned char tolower(unsigned char c)
{
	if (isupper(c))
		c -= 'A'-'a';
	return c;
}

static inline unsigned char toupper(unsigned char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

#endif /* _CTYPE_H */
