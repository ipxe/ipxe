#include "etherboot.h"
#include <stdarg.h>

#define LONG_SHIFT  ((int)((sizeof(unsigned long)*CHAR_BIT) - 4))
#define INT_SHIFT   ((int)((sizeof(unsigned int)*CHAR_BIT) - 4))
#define SHRT_SHIFT  ((int)((sizeof(unsigned short)*CHAR_BIT) - 4))
#define CHAR_SHIFT  ((int)((sizeof(unsigned char)*CHAR_BIT) - 4))

/**************************************************************************
PRINTF and friends

	Formats:
		%[#]x	- 4 bytes int (8 hex digits, lower case)
		%[#]X	- 4 bytes int (8 hex digits, upper case)
		%[#]lx  - 8 bytes long (16 hex digits, lower case)
		%[#]lX  - 8 bytes long (16 hex digits, upper case)
		%[#]hx	- 2 bytes int (4 hex digits, lower case)
		%[#]hX	- 2 bytes int (4 hex digits, upper case)
		%[#]hhx	- 1 byte int (2 hex digits, lower case)
		%[#]hhX	- 1 byte int (2 hex digits, upper case)
			- optional # prefixes 0x or 0X
		%d	- decimal int
		%c	- char
		%s	- string
		%@	- Internet address in ddd.ddd.ddd.ddd notation
		%!	- Ethernet address in xx:xx:xx:xx:xx:xx notation
	Note: width specification ignored
**************************************************************************/
static int vsprintf(char *buf, const char *fmt, va_list args)
{
	char *p, *s;
	s = buf;
	for ( ; *fmt != '\0'; ++fmt) {
		if (*fmt != '%') {
			buf ? *s++ = *fmt : putchar(*fmt);
			continue;
		}
		/* skip width specs */
		fmt++;
		while (*fmt >= '0' && *fmt <= '9')
			fmt++;
		if (*fmt == '.')
			fmt++;
		while (*fmt >= '0' && *fmt <= '9')
			fmt++;
		if (*fmt == 's') {
			for(p = va_arg(args, char *); *p != '\0'; p++) 
				buf ? *s++ = *p : putchar(*p);
		}
		else {	/* Length of item is bounded */
			char tmp[40], *q = tmp;
			int alt = 0;
			int shift = INT_SHIFT;
			if (*fmt == '#') {
				alt = 1;
				fmt++;
			}
			if (*fmt == 'l') {
				shift = LONG_SHIFT;
				fmt++;
			}
			else if (*fmt == 'h') {
				shift = SHRT_SHIFT;
				fmt++;
				if (*fmt == 'h') {
					shift = CHAR_SHIFT;
					fmt++;
				}
			}
			
			/*
			 * Before each format q points to tmp buffer
			 * After each format q points past end of item
			 */
			if ((*fmt | 0x20) == 'x') {
				/* With x86 gcc, sizeof(long) == sizeof(int) */
				unsigned long h;
				int ncase;
				if (shift > INT_SHIFT) {
					h = va_arg(args, unsigned long);
				} else {
					h = va_arg(args, unsigned int);
				}
				ncase = (*fmt & 0x20);
				if (alt) {
					*q++ = '0';
					*q++ = 'X' | ncase;
				}
				for ( ; shift >= 0; shift -= 4)
					*q++ = "0123456789ABCDEF"[(h >> shift) & 0xF] | ncase;
			}
			else if (*fmt == 'd') {
				char *r;
				long i;
				if (shift > INT_SHIFT) {
					i = va_arg(args, long);
				} else {
					i = va_arg(args, int);
				}
				if (i < 0) {
					*q++ = '-';
					i = -i;
				}
				p = q;		/* save beginning of digits */
				do {
					*q++ = '0' + (i % 10);
					i /= 10;
				} while (i);
				/* reverse digits, stop in middle */
				r = q;		/* don't alter q */
				while (--r > p) {
					i = *r;
					*r = *p;
					*p++ = i;
				}
			}
			else if (*fmt == '@') {
				unsigned char *r;
				union {
					uint32_t	l;
					unsigned char	c[4];
				} u;
				u.l = va_arg(args, uint32_t);
				for (r = &u.c[0]; r < &u.c[4]; ++r)
					q += sprintf(q, "%d.", *r);
				--q;
			}
			else if (*fmt == '!') {
				char *r;
				p = va_arg(args, char *);
				for (r = p + ETH_ALEN; p < r; ++p)
					q += sprintf(q, "%hhX:", *p);
				--q;
			}
			else if (*fmt == 'c')
				*q++ = va_arg(args, int);
			else
				*q++ = *fmt;
			/* now output the saved string */
			for (p = tmp; p < q; ++p)
				buf ? *s++ = *p : putchar(*p);
		}
	}
	if (buf)
		*s = '\0';
	return (s - buf);
}

int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);
	return i;
}

void printf(const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf(0, fmt, args);
	va_end(args);
}
