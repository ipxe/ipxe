#include <stddef.h>
#include <stdarg.h>
#include <console.h>
#include <errno.h>
#include <vsprintf.h>

#define CHAR_LEN	1
#define SHORT_LEN	2
#define INT_LEN		3
#define LONG_LEN	4
#define LONGLONG_LEN	5
#define SIZE_T_LEN	6

static uint8_t type_sizes[] = {
	[CHAR_LEN]	= sizeof ( char ),
	[SHORT_LEN]	= sizeof ( short ),
	[INT_LEN]	= sizeof ( int ),
	[LONG_LEN]	= sizeof ( long ),
	[LONGLONG_LEN]	= sizeof ( long long ),
	[SIZE_T_LEN]	= sizeof ( size_t ),
};

/** @file */

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

#define LCASE 0x20
#define ALT_FORM 0x02

static char * format_hex ( char *buf, unsigned long long num, int width,
			   int flags ) {
	char *ptr = buf;
	int case_mod;

	/* Generate the number */
	case_mod = flags & LCASE;
	do {
		*ptr++ = "0123456789ABCDEF"[ num & 0xf ] | case_mod;
		num >>= 4;
	} while ( num );

	/* Zero-pad to width */
	while ( ( ptr - buf ) < width )
		*ptr++ = '0';

	/* Add "0x" or "0X" if alternate form specified */
	if ( flags & ALT_FORM ) {
		*ptr++ = 'X' | case_mod;
		*ptr++ = '0';
	}

	return ptr;
}

static char * format_decimal ( char *buf, signed long num, int width ) {
	char *ptr = buf;
	int negative = 0;

	/* Generate the number */
	if ( num < 0 ) {
		negative = 1;
		num = -num;
	}
	do {
		*ptr++ = '0' + ( num % 10 );
		num /= 10;
	} while ( num );

	/* Add "-" if necessary */
	if ( negative )
		*ptr++ = '-';

	/* Space-pad to width */
	while ( ( ptr - buf ) < width )
		*ptr++ = ' ';

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
int vcprintf ( struct printf_context *ctx, const char *fmt, va_list args ) {
	int flags;
	int width;
	uint8_t *length;
	int character;
	unsigned long long hex;
	signed long decimal;
	char num_buf[32];
	char *ptr;

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
		if ( *fmt == 'c' ) {
			character = va_arg ( args, unsigned int );
			ctx->handler ( ctx, character );
		} else if ( *fmt == 's' ) {
			ptr = va_arg ( args, char * );
			for ( ; *ptr ; ptr++ ) {
				ctx->handler ( ctx, *ptr );
			}
		} else if ( *fmt == 'p' ) {
			hex = ( intptr_t ) va_arg ( args, void * );
			ptr = format_hex ( num_buf, hex, width,
					   ( ALT_FORM | LCASE ) );
			do {
				ctx->handler ( ctx, *(--ptr) );
			} while ( ptr != num_buf );
		} else if ( ( *fmt & ~0x20 ) == 'X' ) {
			flags |= ( *fmt & 0x20 ); /* LCASE */
			if ( *length >= sizeof ( unsigned long long ) ) {
				hex = va_arg ( args, unsigned long long );
			} else if ( *length >= sizeof ( unsigned long ) ) {
				hex = va_arg ( args, unsigned long );
			} else {
				hex = va_arg ( args, unsigned int );
			}
			ptr = format_hex ( num_buf, hex, width, flags );
			do {
				ctx->handler ( ctx, *(--ptr) );
			} while ( ptr != num_buf );
		} else if ( *fmt == 'd' ) {
			if ( *length >= sizeof ( signed long ) ) {
				decimal = va_arg ( args, signed long );
			} else {
				decimal = va_arg ( args, signed int );
			}
			ptr = format_decimal ( num_buf, decimal, width );
			do {
				ctx->handler ( ctx, *(--ptr) );
			} while ( ptr != num_buf );
		} else {
			ctx->handler ( ctx, *fmt );
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
	int len;

	/* Ensure last byte is NUL if a size is specified.  This
	 * catches the case of the buffer being too small, in which
	 * case a trailing NUL would not otherwise be added.
	 */
	if ( size != PRINTF_NO_LENGTH )
		buf[size-1] = '\0';

	/* Hand off to vcprintf */
	ctx.handler = printf_sputc;
	ctx.buf = buf;
	ctx.max_len = size;
	len = vcprintf ( &ctx, fmt, args );

	/* Add trailing NUL */
	printf_sputc ( &ctx, '\0' );

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
