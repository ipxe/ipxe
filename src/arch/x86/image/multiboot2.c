/*
 * Copyright (C) 2016 Star Lab Corp.
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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * Multiboot2 image format
 *
 */

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <multiboot2.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/segment.h>
#include <ipxe/io.h>
#include <ipxe/elf.h>
#include <ipxe/init.h>
#include <ipxe/features.h>
#include <ipxe/uri.h>
#include <ipxe/version.h>

FEATURE ( FEATURE_IMAGE, "MBOOT2", DHCP_EB_FEATURE_MULTIBOOT2, 1 );

/**
 * Execute multiboot2 image
 *
 * @v image		Multiboot image
 * @ret rc		Return status code
 */
static int multiboot2_exec ( struct image *image ) {
	(void)image;
	return -ENOEXEC;
}

/**
 * Probe multiboot2 image
 *
 * @v image		Multiboot file
 * @ret rc		Return status code
 */
static int multiboot2_probe ( struct image *image ) {
	(void)image;
	return -ENOEXEC;
}

/** Multiboot image type */
struct image_type multiboot2_image_type __image_type ( PROBE_MULTIBOOT2 ) = {
	.name = "Multiboot 2",
	.probe = multiboot2_probe,
	.exec = multiboot2_exec,
};
