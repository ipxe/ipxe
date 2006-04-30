#include <stdarg.h>
#include <gpxe/if_ether.h> /* for ETH_ALEN */
#include "limits.h" /* for CHAR_BIT */
#include "console.h"
#include "errno.h"
#include "vsprintf.h"

#define LONG_SHIFT  ((int)((sizeof(unsigned long)*CHAR_BIT) - 4))
#define INT_SHIFT   ((int)((sizeof(unsigned int)*CHAR_BIT) - 4))
#define SHRT_SHIFT  ((int)((sizeof(unsigned short)*CHAR_BIT) - 4))
#define CHAR_SHIFT  ((int)((sizeof(unsigned char)*CHAR_BIT) - 4))

/** @file */

/**
 * Write a formatted string to a buffer.
 *
 * @v buf		Buffer into which to write the string, or NULL
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of string written to buffer (if buf != NULL)
 * @ret	0		(if buf == NULL)
 * @err None		-
 *
 * If #buf==NULL, then the string will be written to the console
 * directly using putchar().
 *
 */
static int vsprintf(char *buf, const char *fmt, va_list args)
{
	const char *p;
	char *s;
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
		} else if (*fmt == 'm') {
			for(p = strerror(errno); *p != '\0'; p++)
				buf ? *s++ = *p : putchar(*p);
		} else {	/* Length of item is bounded */
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
				char *r, *t;
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
				t = q;		/* save beginning of digits */
				do {
					*q++ = '0' + (i % 10);
					i /= 10;
				} while (i);
				/* reverse digits, stop in middle */
				r = q;		/* don't alter q */
				while (--r > t) {
					i = *r;
					*r = *t;
					*t++ = i;
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
				const char *r;
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

/**
 * Write a formatted string to a buffer.
 *
 * @v buf		Buffer into which to write the string, or NULL
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret len		Length of string written to buffer (if buf != NULL)
 * @ret	0		(if buf == NULL)
 * @err None		-
 *
 * If #buf==NULL, then the string will be written to the console
 * directly using putchar().
 *
 */
int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);
	return i;
}

#warning "Remove this buffer-overflow-in-waiting at some point"
int snprintf ( char *buf, size_t size, const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vsprintf ( buf, fmt, args );
	va_end ( args );
	return i;
}

/**
 * Write a formatted string to the console.
 *
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret	None		-
 * @err None		-
 *
 */
int printf(const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf(0, fmt, args);
	va_end(args);
	return i;
}
