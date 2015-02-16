/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/** @file
 *
 * String functions
 *
 */

/**
 * Fill memory region
 *
 * @v dest		Destination region
 * @v character		Fill character
 * @v len		Length
 * @ret dest		Destination region
 */
void * generic_memset ( void *dest, int character, size_t len ) {
	uint8_t *dest_bytes = dest;

	while ( len-- )
		*(dest_bytes++) = character;
	return dest;
}

/**
 * Copy memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 * @ret dest		Destination region
 */
void * generic_memcpy ( void *dest, const void *src, size_t len ) {
	const uint8_t *src_bytes = src;
	uint8_t *dest_bytes = dest;

	while ( len-- )
		*(dest_bytes++) = *(src_bytes++);
	return dest;
}

/**
 * Copy (possibly overlapping) memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 * @ret dest		Destination region
 */
void * generic_memmove ( void *dest, const void *src, size_t len ) {
	const uint8_t *src_bytes = ( src + len );
	uint8_t *dest_bytes = ( dest + len );

	if ( dest < src )
		return memcpy ( dest, src, len );
	while ( len-- )
		*(--dest_bytes) = *(--src_bytes);
	return dest;
}

/**
 * Compare memory regions
 *
 * @v first		First region
 * @v second		Second region
 * @v len		Length
 * @ret diff		Difference
 */
int memcmp ( const void *first, const void *second, size_t len ) {
	const uint8_t *first_bytes = first;
	const uint8_t *second_bytes = second;
	int diff;

	while ( len-- ) {
		diff = ( *(second_bytes++) - *(first_bytes++) );
		if ( diff )
			return diff;
	}
	return 0;
}

/**
 * Find character within a memory region
 *
 * @v src		Source region
 * @v character		Character to find
 * @v len		Length
 * @ret found		Found character, or NULL if not found
 */
void * memchr ( const void *src, int character, size_t len ) {
	const uint8_t *src_bytes = src;

	for ( ; len-- ; src_bytes++ ) {
		if ( *src_bytes == character )
			return ( ( void * ) src_bytes );
	}
	return NULL;
}

/**
 * Swap memory regions
 *
 * @v first		First region
 * @v second		Second region
 * @v len		Length
 * @ret first		First region
 */
void * memswap ( void *first, void *second, size_t len ) {
	uint8_t *first_bytes = first;
	uint8_t *second_bytes = second;
	uint8_t temp;

	for ( ; len-- ; first_bytes++, second_bytes++ ) {
		temp = *first_bytes;
		*first_bytes = *second_bytes;
		*second_bytes = temp;
	}
	return first;
}

/**
 * Compare strings
 *
 * @v first		First string
 * @v second		Second string
 * @ret diff		Difference
 */
int strcmp ( const char *first, const char *second ) {

	return strncmp ( first, second, ~( ( size_t ) 0 ) );
}

/**
 * Compare strings
 *
 * @v first		First string
 * @v second		Second string
 * @v max		Maximum length to compare
 * @ret diff		Difference
 */
int strncmp ( const char *first, const char *second, size_t max ) {
	const uint8_t *first_bytes = ( ( const uint8_t * ) first );
	const uint8_t *second_bytes = ( ( const uint8_t * ) second );
	int diff;

	for ( ; max-- ; first_bytes++, second_bytes++ ) {
		diff = ( *second_bytes - *first_bytes );
		if ( diff )
			return diff;
		if ( ! *first_bytes )
			return 0;
	}
	return 0;
}

/**
 * Compare case-insensitive strings
 *
 * @v first		First string
 * @v second		Second string
 * @ret diff		Difference
 */
int strcasecmp ( const char *first, const char *second ) {
	const uint8_t *first_bytes = ( ( const uint8_t * ) first );
	const uint8_t *second_bytes = ( ( const uint8_t * ) second );
	int diff;

	for ( ; ; first_bytes++, second_bytes++ ) {
		diff = ( toupper ( *second_bytes ) -
			 toupper ( *first_bytes ) );
		if ( diff )
			return diff;
		if ( ! *first_bytes )
			return 0;
	}
}

/**
 * Get length of string
 *
 * @v src		String
 * @ret len		Length
 */
size_t strlen ( const char *src ) {

	return strnlen ( src, ~( ( size_t ) 0 ) );
}

/**
 * Get length of string
 *
 * @v src		String
 * @v max		Maximum length
 * @ret len		Length
 */
size_t strnlen ( const char *src, size_t max ) {
	const uint8_t *src_bytes = ( ( const uint8_t * ) src );
	size_t len = 0;

	while ( max-- && *(src_bytes++) )
		len++;
	return len;
}

/**
 * Find character within a string
 *
 * @v src		String
 * @v character		Character to find
 * @ret found		Found character, or NULL if not found
 */
char * strchr ( const char *src, int character ) {
	const uint8_t *src_bytes = ( ( const uint8_t * ) src );

	for ( ; ; src_bytes++ ) {
		if ( *src_bytes == character )
			return ( ( char * ) src_bytes );
		if ( ! *src_bytes )
			return NULL;
	}
}

/**
 * Find rightmost character within a string
 *
 * @v src		String
 * @v character		Character to find
 * @ret found		Found character, or NULL if not found
 */
char * strrchr ( const char *src, int character ) {
	const uint8_t *src_bytes = ( ( const uint8_t * ) src );
	const uint8_t *start = src_bytes;

	while ( *src_bytes )
		src_bytes++;
	for ( src_bytes-- ; src_bytes >= start ; src_bytes-- ) {
		if ( *src_bytes == character )
			return ( ( char * ) src_bytes );
	}
	return NULL;
}

/**
 * Find substring
 *
 * @v haystack		String
 * @v needle		Substring
 * @ret found		Found substring, or NULL if not found
 */
char * strstr ( const char *haystack, const char *needle ) {
	size_t len = strlen ( needle );

	for ( ; *haystack ; haystack++ ) {
		if ( memcmp ( haystack, needle, len ) == 0 )
			return ( ( char * ) haystack );
	}
	return NULL;
}

/**
 * Copy string
 *
 * @v dest		Destination string
 * @v src		Source string
 * @ret dest		Destination string
 */
char * strcpy ( char *dest, const char *src ) {
	const uint8_t *src_bytes = ( ( const uint8_t * ) src );
	uint8_t *dest_bytes = ( ( uint8_t * ) dest );

	/* We cannot use strncpy(), since that would pad the destination */
	for ( ; ; src_bytes++, dest_bytes++ ) {
		*dest_bytes = *src_bytes;
		if ( ! *dest_bytes )
			break;
	}
	return dest;
}

/**
 * Copy string
 *
 * @v dest		Destination string
 * @v src		Source string
 * @v max		Maximum length
 * @ret dest		Destination string
 */
char * strncpy ( char *dest, const char *src, size_t max ) {
	const uint8_t *src_bytes = ( ( const uint8_t * ) src );
	uint8_t *dest_bytes = ( ( uint8_t * ) dest );

	for ( ; max ; max--, src_bytes++, dest_bytes++ ) {
		*dest_bytes = *src_bytes;
		if ( ! *dest_bytes )
			break;
	}
	while ( max-- )
		*(dest_bytes++) = '\0';
	return dest;
}

/**
 * Concatenate string
 *
 * @v dest		Destination string
 * @v src		Source string
 * @ret dest		Destination string
 */
char * strcat ( char *dest, const char *src ) {

	strcpy ( ( dest + strlen ( dest ) ), src );
	return dest;
}

/**
 * Duplicate string
 *
 * @v src		Source string
 * @ret dup		Duplicated string, or NULL if allocation failed
 */
char * strdup ( const char *src ) {

	return strndup ( src, ~( ( size_t ) 0 ) );
}

/**
 * Duplicate string
 *
 * @v src		Source string
 * @v max		Maximum length
 * @ret dup		Duplicated string, or NULL if allocation failed
 */
char * strndup ( const char *src, size_t max ) {
	size_t len = strnlen ( src, max );
        char *dup;

        dup = malloc ( len + 1 /* NUL */ );
        if ( dup ) {
		memcpy ( dup, src, len );
		dup[len] = '\0';
        }
        return dup;
}
