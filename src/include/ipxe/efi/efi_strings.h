#ifndef _IPXE_EFI_STRINGS_H
#define _IPXE_EFI_STRINGS_H

/** @file
 *
 * EFI strings
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

extern int efi_vsnprintf ( wchar_t *wbuf, size_t wsize, const char *fmt,
			   va_list args );
extern int efi_snprintf ( wchar_t *wbuf, size_t wsize, const char *fmt, ... );
extern int efi_vssnprintf ( wchar_t *wbuf, ssize_t swsize, const char *fmt,
			    va_list args );
extern int efi_ssnprintf ( wchar_t *wbuf, ssize_t swsize,
			   const char *fmt, ... );

/**
 * Write a formatted string to a wide-character buffer
 *
 * @v wbuf		Buffer into which to write the string
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret wlen		Length of formatted string (in wide characters)
 */
static inline int efi_vsprintf ( wchar_t *buf, const char *fmt, va_list args ) {
	return efi_vsnprintf ( buf, ~( ( size_t ) 0 ), fmt, args );
}

/**
 * Write a formatted string to a buffer
 *
 * @v wbuf		Buffer into which to write the string
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret wlen		Length of formatted string (in wide characters)
 */
#define efi_sprintf( buf, fmt, ... ) \
	efi_snprintf ( (buf), ~( ( size_t ) 0 ), (fmt), ## __VA_ARGS__ )

#endif /* _IPXE_EFI_STRINGS_H */
