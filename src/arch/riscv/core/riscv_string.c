/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

/** @file
 *
 * Optimised string operations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>

/**
 * Copy memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void riscv_memcpy ( void *dest, const void *src, size_t len ) {
	size_t len_pre;
	size_t len_mid;
	size_t len_post;
	unsigned long discard_data;

	/* Calculate pre-aligned, aligned, and post-aligned lengths.
	 * (Align on the destination address, on the assumption that
	 * misaligned stores are likely to be more expensive than
	 * misaligned loads.)
	 */
	len_pre = ( ( sizeof ( unsigned long ) - ( ( intptr_t ) dest ) ) &
		    ( sizeof ( unsigned long ) - 1 ) );
	if ( len_pre > len )
		len_pre = len;
	len -= len_pre;
	len_mid = ( len & ~( sizeof ( unsigned long ) - 1 ) );
	len -= len_mid;
	len_post = len;

	/* Copy pre-aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       "lb %2, (%1)\n\t"
			       "sb %2, (%0)\n\t"
			       "addi %0, %0, 1\n\t"
			       "addi %1, %1, 1\n\t"
			       "\n2:\n\t"
			       "bne %0, %3, 1b\n\t"
			       : "+r" ( dest ), "+r" ( src ),
				 "=&r" ( discard_data )
			       : "r" ( dest + len_pre )
			       : "memory" );

	/* Copy aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       LOADN " %2, (%1)\n\t"
			       STOREN " %2, (%0)\n\t"
			       "addi %0, %0, %4\n\t"
			       "addi %1, %1, %4\n\t"
			       "\n2:\n\t"
			       "bne %0, %3, 1b\n\t"
			       : "+r" ( dest ), "+r" ( src ),
				 "=&r" ( discard_data )
			       : "r" ( dest + len_mid ),
				 "i" ( sizeof ( unsigned long ) )
			       : "memory" );

	/* Copy post-aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       "lb %2, (%1)\n\t"
			       "sb %2, (%0)\n\t"
			       "addi %0, %0, 1\n\t"
			       "addi %1, %1, 1\n\t"
			       "\n2:\n\t"
			       "bne %0, %3, 1b\n\t"
			       : "+r" ( dest ), "+r" ( src ),
				 "=&r" ( discard_data )
			       : "r" ( dest + len_post )
			       : "memory" );
}

/**
 * Zero memory region
 *
 * @v dest		Destination region
 * @v len		Length
 */
void riscv_bzero ( void *dest, size_t len ) {
	size_t len_pre;
	size_t len_mid;
	size_t len_post;

	/* Calculate pre-aligned, aligned, and post-aligned lengths */
	len_pre = ( ( sizeof ( unsigned long ) - ( ( intptr_t ) dest ) ) &
		    ( sizeof ( unsigned long ) - 1 ) );
	if ( len_pre > len )
		len_pre = len;
	len -= len_pre;
	len_mid = ( len & ~( sizeof ( unsigned long ) - 1 ) );
	len -= len_mid;
	len_post = len;

	/* Zero pre-aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       "sb zero, (%0)\n\t"
			       "addi %0, %0, 1\n\t"
			       "\n2:\n\t"
			       "bne %0, %1, 1b\n\t"
			       : "+r" ( dest )
			       : "r" ( dest + len_pre )
			       : "memory" );

	/* Zero aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       STOREN " zero, (%0)\n\t"
			       "addi %0, %0, %2\n\t"
			       "\n2:\n\t"
			       "bne %0, %1, 1b\n\t"
			       : "+r" ( dest )
			       : "r" ( dest + len_mid ),
				 "i" ( sizeof ( unsigned long ) )
			       : "memory" );

	/* Zero post-aligned section */
	__asm__ __volatile__ ( "j 2f\n\t"
			       "\n1:\n\t"
			       "sb zero, (%0)\n\t"
			       "addi %0, %0, 1\n\t"
			       "\n2:\n\t"
			       "bne %0, %1, 1b\n\t"
			       : "+r" ( dest )
			       : "r" ( dest + len_post )
			       : "memory" );
}

/**
 * Fill memory region
 *
 * @v dest		Destination region
 * @v len		Length
 * @v character		Fill character
 *
 * The unusual parameter order is to allow for more efficient
 * tail-calling to riscv_bzero() when zeroing a region.
 */
void riscv_memset ( void *dest, size_t len, int character ) {

	/* Do nothing if length is zero */
	if ( ! len )
		return;

	/* Use optimised zeroing code if applicable */
	if ( character == 0 ) {
		riscv_bzero ( dest, len );
		return;
	}

	/* Fill one byte at a time.  Calling memset() with a non-zero
	 * value is relatively rare and unlikely to be
	 * performance-critical.
	 */
	__asm__ __volatile__ ( "\n1:\n\t"
			       "sb %2, (%0)\n\t"
			       "addi %0, %0, 1\n\t"
			       "bne %0, %1, 1b\n\t"
			       : "+r" ( dest )
			       : "r" ( dest + len ), "r" ( character )
			       : "memory" );
}

/**
 * Copy (possibly overlapping) memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 */
void riscv_memmove ( void *dest, const void *src, size_t len ) {
	void *orig_dest = dest;
	unsigned long discard_data;

	/* Do nothing if length is zero */
	if ( ! len )
		return;

	/* Use memcpy() if copy direction is forwards */
	if ( dest <= src ) {
		memcpy ( dest, src, len );
		return;
	}

	/* Assume memmove() is not performance-critical, and perform a
	 * bytewise copy backwards for simplicity.
	 */
	dest += len;
	src += len;
	__asm__ __volatile__ ( "\n1:\n\t"
			       "addi %1, %1, -1\n\t"
			       "addi %0, %0, -1\n\t"
			       "lb %2, (%1)\n\t"
			       "sb %2, (%0)\n\t"
			       "bne %0, %3, 1b\n\t"
			       : "+r" ( dest ), "+r" ( src ),
				 "=&r" ( discard_data )
			       : "r" ( orig_dest )
			       : "memory" );
}
