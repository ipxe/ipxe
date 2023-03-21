#ifndef _IPXE_UTF8_H
#define _IPXE_UTF8_H

/** @file
 *
 * UTF-8 Unicode encoding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** Maximum length of UTF-8 sequence */
#define UTF8_MAX_LEN 4

/** Minimum legal value for two-byte UTF-8 sequence */
#define UTF8_MIN_TWO 0x80

/** Minimum legal value for three-byte UTF-8 sequence */
#define UTF8_MIN_THREE 0x800

/** Minimum legal value for four-byte UTF-8 sequence */
#define UTF8_MIN_FOUR 0x10000

/** High bit of UTF-8 bytes */
#define UTF8_HIGH_BIT 0x80

/** Number of data bits in each continuation byte */
#define UTF8_CONTINUATION_BITS 6

/** Bit mask for data bits in a continuation byte */
#define UTF8_CONTINUATION_MASK ( ( 1 << UTF8_CONTINUATION_BITS ) - 1 )

/** Non-data bits in a continuation byte */
#define UTF8_CONTINUATION 0x80

/** Check for a continuation byte
 *
 * @v byte		UTF-8 byte
 * @ret is_continuation	Byte is a continuation byte
 */
#define UTF8_IS_CONTINUATION( byte ) \
	( ( (byte) & ~UTF8_CONTINUATION_MASK ) == UTF8_CONTINUATION )

/** Check for an ASCII byte
 *
 * @v byte		UTF-8 byte
 * @ret is_ascii	Byte is an ASCII byte
 */
#define UTF8_IS_ASCII( byte ) ( ! ( (byte) & UTF8_HIGH_BIT ) )

/** Invalid character returned when decoding fails */
#define UTF8_INVALID 0xfffd

/** A UTF-8 character accumulator */
struct utf8_accumulator {
	/** Character in progress */
	unsigned int character;
	/** Number of remaining continuation bytes */
	unsigned int remaining;
	/** Minimum legal character */
	unsigned int min;
};

extern unsigned int utf8_accumulate ( struct utf8_accumulator *utf8,
				      uint8_t byte );

#endif /* _IPXE_UTF8_H */
