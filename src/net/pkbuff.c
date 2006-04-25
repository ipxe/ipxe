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
#include <malloc.h>
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
 */
struct pk_buff * alloc_pkb ( size_t len ) {
	struct pk_buff *pkb = NULL;

	/* Allocate memory for buffer plus descriptor */
	pkb = malloc ( sizeof ( *pkb ) + len );
	if ( pkb ) {
		pkb->head = pkb->data = pkb->tail = ( void * ) ( pkb + 1 );
		pkb->end = pkb->head + len;
	}
	return pkb;
}

/**
 * Free packet buffer
 *
 * @v pkb	Packet buffer, or NULL
 */
void free_pkb ( struct pk_buff *pkb ) {
	free ( pkb );
}
