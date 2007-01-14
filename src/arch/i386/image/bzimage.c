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
 * Linux bzImage image format
 *
 */

#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <bzimage.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/memmap.h>
#include <gpxe/shutdown.h>

struct image_type bzimage_image_type __image_type ( PROBE_NORMAL );

/**
 * Execute bzImage image
 *
 * @v image		bzImage image
 * @ret rc		Return status code
 */
static int bzimage_exec ( struct image *image ) {
}

/**
 * Load bzImage image into memory
 *
 * @v image		bzImage file
 * @ret rc		Return status code
 */
int bzimage_load ( struct image *image ) {
	struct bzimage_header bzhdr;

	/* Sanity check */
	if ( image->len < ( BZHDR_OFFSET + sizeof ( bzhdr ) ) ) {
		DBGC ( image, "BZIMAGE %p too short\n", image );
		return -ENOEXEC;
	}

	/* Read and verify header */
	copy_from_user ( &bzhdr, image->data, BZHDR_OFFSET, sizeof ( bzhdr ) );
	if ( bzhdr.header != BZIMAGE_SIGNATURE ) {
		DBGC ( image, "BZIMAGE %p not a bzImage\n", image );
		return -ENOEXEC;
	}

	/* This is a bzImage image, valid or otherwise */
	if ( ! image->type )
		image->type = &bzimage_image_type;

	return 0;
}

/** Linux bzImage image type */
struct image_type bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.load = bzimage_load,
	.exec = bzimage_exec,
};
