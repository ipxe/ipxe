/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * Packet buffer padding
 *
 */

#include <string.h>
#include <gpxe/pkbuff.h>

/**
 * Pad packet buffer
 *
 * @v pkb		Packet buffer
 * @v min_len		Minimum length
 *
 * This function pads and aligns packet buffers, for devices that
 * aren't capable of padding in hardware, or that require specific
 * alignment in TX buffers.  The packet data will end up aligned to a
 * multiple of @c PKB_ALIGN.
 *
 * @c min_len must not exceed @v PKB_ZLEN.
 */
void pkb_pad ( struct pk_buff *pkb, size_t min_len ) {
	void *data;
	size_t len;
	size_t headroom;
	signed int pad_len;

	assert ( min_len <= PKB_ZLEN );

	/* Move packet data to start of packet buffer.  This will both
	 * align the data (since packet buffers are aligned to
	 * PKB_ALIGN) and give us sufficient space for the
	 * zero-padding
	 */
	data = pkb->data;
	len = pkb_len ( pkb );
	headroom = pkb_headroom ( pkb );
	pkb_push ( pkb, headroom );
	memmove ( pkb->data, data, len );
	pkb_unput ( pkb, headroom );

	/* Pad to minimum packet length */
	pad_len = ( min_len - pkb_len ( pkb ) );
	if ( pad_len > 0 )
		memset ( pkb_put ( pkb, pad_len ), 0, pad_len );
}
