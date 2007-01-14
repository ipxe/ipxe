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
 * Linux initrd image format
 *
 * This file does nothing except provide a way to mark images as being
 * initrds.  The actual processing is done in the Linux kernel image
 * code; this file exists so that we can include the "initrd" command
 * without necessarily dragging in the Linux image format.
 *
 */

#include <gpxe/image.h>
#include <gpxe/initrd.h>

/** Linux initrd image type */
struct image_type initrd_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "initrd",
};
