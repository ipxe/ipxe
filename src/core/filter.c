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

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/xfer.h>
#include <gpxe/filter.h>

/** @file
 *
 * Data transfer filters
 *
 */

/*
 * Pass-through methods to be used by filters which don't want to
 * intercept all events.
 *
 */

void filter_close ( struct xfer_interface *xfer, int rc ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	xfer_close ( other, rc );
}

int filter_vredirect ( struct xfer_interface *xfer, int type,
			va_list args ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	return xfer_vredirect ( other, type, args );
}

size_t filter_window ( struct xfer_interface *xfer ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	return xfer_window ( other );
}

struct io_buffer * filter_alloc_iob ( struct xfer_interface *xfer,
				      size_t len ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	return xfer_alloc_iob ( other, len );
}

int filter_deliver_iob ( struct xfer_interface *xfer, struct io_buffer *iobuf,
			 struct xfer_metadata *meta ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	return xfer_deliver_iob_meta ( other, iobuf, meta );
}

int filter_deliver_raw ( struct xfer_interface *xfer, const void *data,
			 size_t len ) {
	struct xfer_interface *other = filter_other_half ( xfer );

	return xfer_deliver_raw ( other, data, len );
}
