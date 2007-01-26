#ifndef _CTYPE_H
#define _CTYPE_H

/** @file
 *
 * Character types
 */

#define isdigit(c)	((c & 0x04) != 0)
#define islower(c)	((c & 0x02) != 0)
//#define isspace(c)	((c & 0x20) != 0)
#define isupper(c)	((c & 0x01) != 0)

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
