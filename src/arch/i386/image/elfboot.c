/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <elf.h>
#include <gpxe/image.h>
#include <gpxe/elf.h>
#include <gpxe/features.h>
#include <gpxe/init.h>

/**
 * @file
 *
 * ELF bootable image
 *
 */

FEATURE ( FEATURE_IMAGE, "ELF", DHCP_EB_FEATURE_ELF, 1 );

struct image_type elfboot_image_type __image_type ( PROBE_NORMAL );

/**
 * Execute ELF image
 *
 * @v image		ELF image
 * @ret rc		Return status code
 */
static int elfboot_exec ( struct image *image ) {
	physaddr_t entry = image->priv.phys;

	/* An ELF image has no callback interface, so we need to shut
	 * down before invoking it.
	 */
	shutdown ( SHUTDOWN_BOOT );

	/* Jump to OS with flat physical addressing */
	DBGC ( image, "ELF %p starting execution at %lx\n", image, entry );
	__asm__ __volatile__ ( PHYS_CODE ( "call *%%edi\n\t" )
			       : : "D" ( entry )
			       : "eax", "ebx", "ecx", "edx", "esi", "ebp",
			         "memory" );

	DBGC ( image, "ELF %p returned\n", image );

	/* It isn't safe to continue after calling shutdown() */
	while ( 1 ) {}

	return -ECANCELED;  /* -EIMPOSSIBLE, anyone? */
}

/**
 * Load ELF image into memory
 *
 * @v image		ELF file
 * @ret rc		Return status code
 */
static int elfboot_load ( struct image *image ) {
	Elf32_Ehdr ehdr;
	static const uint8_t e_ident[] = {
		[EI_MAG0]	= ELFMAG0,
		[EI_MAG1]	= ELFMAG1,
		[EI_MAG2]	= ELFMAG2,
		[EI_MAG3]	= ELFMAG3,
		[EI_CLASS]	= ELFCLASS32,
		[EI_DATA]	= ELFDATA2LSB,
		[EI_VERSION]	= EV_CURRENT,
	};
	int rc;

	/* Read ELF header */
	copy_from_user ( &ehdr, image->data, 0, sizeof ( ehdr ) );
	if ( memcmp ( ehdr.e_ident, e_ident, sizeof ( e_ident ) ) != 0 ) {
		DBG ( "Invalid ELF identifier\n" );
		return -ENOEXEC;
	}

	/* This is an ELF image, valid or otherwise */
	if ( ! image->type )
		image->type = &elfboot_image_type;

	/* Load the image using core ELF support */
	if ( ( rc = elf_load ( image ) ) != 0 ) {
		DBGC ( image, "ELF %p could not load: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** ELF image type */
struct image_type elfboot_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "ELF",
	.load = elfboot_load,
	.exec = elfboot_exec,
};
