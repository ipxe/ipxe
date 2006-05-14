/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stddef.h>
#include <stdarg.h>
#include <console.h>
#include <errno.h>
#include <vsprintf.h>

/** @file */

#define CHAR_LEN	0	/**< "hh" length modifier */
#define SHORT_LEN	1	/**< "h" length modifier */
#define INT_LEN		2	/**< no length modifier */
#define LONG_LEN	3	/**< "l" length modifier */
#define LONGLONG_LEN	4	/**< "ll" length modifier */
#define SIZE_T_LEN	5	/**< "z" length modifier */

static uint8_t type_sizes[] = {
	[CHAR_LEN]	= sizeof ( char ),
	[SHORT_LEN]	= sizeof ( short ),
	[INT_LEN]	= sizeof ( int ),
	[LONG_LEN]	= sizeof ( long ),
	[LONGLONG_LEN]	= sizeof ( long long ),
	[SIZE_T_LEN]	= sizeof ( size_t ),
};

/**
 * A printf context
 *
 * Contexts are used in order to be able to share code between
 * vprintf() and vsnprintf(), without requiring the allocation of a
 * buffer for vprintf().
 */
struct printf_context {
	/**
	 * Character handler
	 *
	 * @v ctx	Context
	 * @v c		Character
	 *
	 * This method is called for each character written to the
	 * formatted string.  It must increment @len.
	 */
	void ( * handler ) ( struct printf_context *ctx, unsigned int c );
	/** Length of formatted string */
	size_t len;
	/** Buffer for formatted string (used by printf_sputc()) */
	char *buf;
	/** Buffer length (used by printf_sputc()) */
	size_t max_len;
};

/**
 * Use lower-case for hexadecimal digits
 *
 * Note that this value is set to 0x20 since that makes for very
 * efficient calculations.  (Bitwise-ORing with @c LCASE converts to a
 * lower-case character, for example.)
 */
#define LCASE 0x20

/**
 * Use "alternate form"
 *
 * For hexadecimal numbers, this means to add a "0x" or "0X" prefix to
 * the number.
 */
#define ALT_FORM 0x02

/**
 * Format a hexadecimal number
 *
 * @v end		End of buffer to contain number
 * @v num		Number to format
 * @v width		Minimum field width
 * @ret ptr		End of buffer
 *
 * Fills a buffer in reverse order with a formatted hexadecimal
 * number.  The number will be zero-padded to the specified width.
 * Lower-case and "alternate form" (i.e. "0x" prefix) flags may be
 * set.
 *
 * There must be enough space in the buffer to contain the largest
 * number that this function can format.
 */
static char * format_hex ( char *end, unsigned long long num, int width,
			   int flags ) {
	char *ptr = end;
	int case_mod;

	/* Generate the number */
	case_mod = flags & LCASE;
	do {
		*(--ptr) = "0123456789ABCDEF"[ num & 0xf ] | case_mod;
		num >>= 4;
	} while ( num );

	/* Zero-pad to width */
	while ( ( end - ptr ) < width )
		*(--ptr) = '0';

	/* Add "0x" or "0X" if alternate form specified */
	if ( flags & ALT_FORM ) {
		*(--ptr) = 'X' | case_mod;
		*(--ptr) = '0';
	}

	return ptr;
}

/**
 * Format a decimal number
 *
 * @v end		End of buffer to contain number
 * @v num		Number to format
 * @v width		Minimum field width
 * @ret ptr		End of buffer
 *
 * Fills a buffer in reverse order with a formatted decimal number.
 * The number will be space-padded to the specified width.
 *
 * There must be enough space in the buffer to contain the largest
 * number that this function can format.
 */
static char * format_decimal ( char *end, signed long num, int width ) {
	char *ptr = end;
	int negative = 0;

	/* Generate the number */
	if ( num < 0 ) {
		negative = 1;
		num = -num;
	}
	do {
		*(--ptr) = '0' + ( num % 10 );
		num /= 10;
	} while ( num );

	/* Add "-" if necessary */
	if ( negative )
		*(--ptr) = '-';

	/* Space-pad to width */
	while ( ( end - ptr ) < width )
		*(--ptr) = ' ';

	return ptr;
}

/**
 * Write a formatted string to a printf context
 *
 * @v ctx		Context
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
size_t vcprintf ( struct printf_context *ctx, const char *fmt, va_list args ) {
	int flags;
	int width;
	uint8_t *length;
	char *ptr;
	char tmp_buf[32]; /* 32 is enough for all numerical formats.
			   * Insane width fields could overflow this buffer. */

	/* Initialise context */
	ctx->len = 0;

	for ( ; *fmt ; fmt++ ) {
		/* Pass through ordinary characters */
		if ( *fmt != '%' ) {
			ctx->handler ( ctx, *fmt );
			continue;
		}
		fmt++;
		/* Process flag characters */
		flags = 0;
		for ( ; ; fmt++ ) {
			if ( *fmt == '#' ) {
				flags |= ALT_FORM;
			} else if ( *fmt == '0' ) {
				/* We always 0-pad hex and space-pad decimal */
			} else {
				/* End of flag characters */
				break;
			}
		}
		/* Process field width */
		width = 0;
		for ( ; ; fmt++ ) {
			if ( ( ( unsigned ) ( *fmt - '0' ) ) < 10 ) {
				width = ( width * 10 ) + ( *fmt - '0' );
			} else {
				break;
			}
		}
		/* We don't do floating point */
		/* Process length modifier */
		length = &type_sizes[INT_LEN];
		for ( ; ; fmt++ ) {
			if ( *fmt == 'h' ) {
				length--;
			} else if ( *fmt == 'l' ) {
				length++;
			} else if ( *fmt == 'z' ) {
				length = &type_sizes[SIZE_T_LEN];
			} else {
				break;
			}
		}
		/* Process conversion specifier */
		ptr = tmp_buf + sizeof ( tmp_buf ) - 1;
		*ptr = '\0';
		if ( *fmt == 'c' ) {
			*(--ptr) = va_arg ( args, unsigned int );
		} else if ( *fmt == 's' ) {
			ptr = va_arg ( args, char * );
		} else if ( *fmt == 'p' ) {
			intptr_t ptrval;

			ptrval = ( intptr_t ) va_arg ( args, void * );
			ptr = format_hex ( ptr, ptrval, width, 
					   ( ALT_FORM | LCASE ) );
		} else if ( ( *fmt & ~0x20 ) == 'X' ) {
			unsigned long long hex;

			flags |= ( *fmt & 0x20 ); /* LCASE */
			if ( *length >= sizeof ( unsigned long long ) ) {
				hex = va_arg ( args, unsigned long long );
			} else if ( *length >= sizeof ( unsigned long ) ) {
				hex = va_arg ( args, unsigned long );
			} else {
				hex = va_arg ( args, unsigned int );
			}
			ptr = format_hex ( ptr, hex, width, flags );
		} else if ( *fmt == 'd' ) {
			signed long decimal;

			if ( *length >= sizeof ( signed long ) ) {
				decimal = va_arg ( args, signed long );
			} else {
				decimal = va_arg ( args, signed int );
			}
			ptr = format_decimal ( ptr, decimal, width );
		} else {
			*(--ptr) = *fmt;
		}
		/* Write out conversion result */
		for ( ; *ptr ; ptr++ ) {
			ctx->handler ( ctx, *ptr );
		}
	}

	return ctx->len;
}

/**
 * Write character to buffer
 *
 * @v ctx		Context
 * @v c			Character
 */
static void printf_sputc ( struct printf_context *ctx, unsigned int c ) {
	if ( ++ctx->len < ctx->max_len )
		ctx->buf[ctx->len-1] = c;
}

/**
 * Write a formatted string to a buffer
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 *
 * If the buffer is too small to contain the string, the returned
 * length is the length that would have been written had enough space
 * been available.
 */
int vsnprintf ( char *buf, size_t size, const char *fmt, va_list args ) {
	struct printf_context ctx;
	size_t len;
	size_t end;

	/* Hand off to vcprintf */
	ctx.handler = printf_sputc;
	ctx.buf = buf;
	ctx.max_len = size;
	len = vcprintf ( &ctx, fmt, args );

	/* Add trailing NUL */
	if ( size ) {
		end = size - 1;
		if ( len < end )
			end = len;
		buf[end] = '\0';
	}

	return len;
}

/**
 * Write a formatted string to a buffer
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int snprintf ( char *buf, size_t size, const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vsnprintf ( buf, size, fmt, args );
	va_end ( args );
	return i;
}

/**
 * Write character to console
 *
 * @v ctx		Context
 * @v c			Character
 */
static void printf_putchar ( struct printf_context *ctx, unsigned int c ) {
	++ctx->len;
	putchar ( c );
}

/**
 * Write a formatted string to the console
 *
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int vprintf ( const char *fmt, va_list args ) {
	struct printf_context ctx;

	/* Hand off to vcprintf */
	ctx.handler = printf_putchar;	
	return vcprintf ( &ctx, fmt, args );	
}

/**
 * Write a formatted string to the console.
 *
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret	len		Length of formatted string
 */
int printf ( const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vprintf ( fmt, args );
	va_end ( args );
	return i;
}
