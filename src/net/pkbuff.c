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

#include <stdint.h>
#include <gpxe/malloc.h>
#include <gpxe/pkbuff.h>

/** @file
 *
 * Packet buffers
 *
 */

/**
 * Allocate packet buffer
 *
 * @v len	Required length of buffer
 * @ret pkb	Packet buffer, or NULL if none available
 *
 * The packet buffer will be physically aligned to a multiple of
 * @c PKBUFF_SIZE.
 */
struct pk_buff * alloc_pkb ( size_t len ) {
	struct pk_buff *pkb = NULL;
	void *data;

	/* Pad to minimum length */
	if ( len < PKB_ZLEN )
		len = PKB_ZLEN;

	/* Align buffer length */
	len = ( len + __alignof__( *pkb ) - 1 ) & ~( __alignof__( *pkb ) - 1 );
	
	/* Allocate memory for buffer plus descriptor */
	data = malloc_dma ( len + sizeof ( *pkb ), PKBUFF_ALIGN );
	if ( ! data )
		return NULL;

	pkb = ( struct pk_buff * ) ( data + len );
	pkb->head = pkb->data = pkb->tail = data;
	pkb->end = pkb;
	return pkb;
}

/**
 * Free packet buffer
 *
 * @v pkb	Packet buffer
 */
void free_pkb ( struct pk_buff *pkb ) {
	if ( pkb ) {
		assert ( pkb->head <= pkb->data );
		assert ( pkb->data <= pkb->tail );
		assert ( pkb->tail <= pkb->end );
		free_dma ( pkb->head,
			   ( pkb->end - pkb->head ) + sizeof ( *pkb ) );
	}
}
