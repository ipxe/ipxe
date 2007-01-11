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
 * Multiboot image format
 *
 */

#include <errno.h>
#include <multiboot.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/elf.h>

/** Boot modules must be page aligned */
#define MB_FLAG_PGALIGN 0x00000001

/** Memory map must be provided */
#define MB_FLAG_MEMMAP 0x00000002

/** Video mode information must be provided */
#define MB_FLAG_VIDMODE 0x00000004

/** Image is a raw multiboot image (not ELF) */
#define MB_FLAG_RAW 0x00010000

/** Multiboot flags that we support */
#define MB_SUPPORTED_FLAGS ( MB_FLAG_PGALIGN | MB_FLAG_MEMMAP | \
			     MB_FLAG_VIDMODE | MB_FLAG_RAW )

/** Compulsory feature multiboot flags */
#define MB_COMPULSORY_FLAGS 0x0000ffff

/** Optional feature multiboot flags */
#define MB_OPTIONAL_FLAGS 0xffff0000

/**
 * Multiboot flags that we don't support
 *
 * We only care about the compulsory feature flags (bits 0-15); we are
 * allowed to ignore the optional feature flags.
 */
#define MB_UNSUPPORTED_FLAGS ( MB_COMPULSORY_FLAGS & ~MB_SUPPORTED_FLAGS )

/** A multiboot header descriptor */
struct multiboot_header_info {
	/** The actual multiboot header */
	struct multiboot_header mb;
	/** Offset of header within the multiboot image */
	size_t offset;
};

/**
 * Execute multiboot image
 *
 * @v image		ELF file
 * @ret rc		Return status code
 */
static int multiboot_execute ( struct image *image __unused ) {
	return -ENOTSUP;
}

/**
 * Find multiboot header
 *
 * @v image		Multiboot file
 * @v hdr		Multiboot header descriptor to fill in
 * @ret rc		Return status code
 */
static int multiboot_find_header ( struct image *image,
				   struct multiboot_header_info *hdr ) {
	uint32_t buf[64];
	size_t offset;
	unsigned int buf_idx;
	uint32_t checksum;

	/* Scan through first 8kB of image file 256 bytes at a time.
	 * (Use the buffering to avoid the overhead of a
	 * copy_from_user() for every dword.)
	 */
	for ( offset = 0 ; offset < 8192 ; offset += sizeof ( buf[0] ) ) {
		/* Check for end of image */
		if ( offset > image->len )
			break;
		/* Refill buffer if applicable */
		buf_idx = ( ( offset % sizeof ( buf ) ) / sizeof ( buf[0] ) );
		if ( buf_idx == 0 ) {
			copy_from_user ( buf, image->data, offset,
					 sizeof ( buf ) );
		}
		/* Check signature */
		if ( buf[buf_idx] != MULTIBOOT_HEADER_MAGIC )
			continue;
		/* Copy header and verify checksum */
		copy_from_user ( &hdr->mb, image->data, offset,
				 sizeof ( hdr->mb ) );
		checksum = ( hdr->mb.magic + hdr->mb.flags +
			     hdr->mb.checksum );
		if ( checksum != 0 )
			continue;
		/* Record offset of multiboot header and return */
		hdr->offset = offset;
		return 0;
	}

	/* No multiboot header found */
	return -ENOEXEC;
}

/**
 * Load raw multiboot image into memory
 *
 * @v image		Multiboot file
 * @v hdr		Multiboot header descriptor
 * @ret rc		Return status code
 */
static int multiboot_load_raw ( struct image *image,
				struct multiboot_header_info *hdr ) {
	size_t offset;
	size_t filesz;
	size_t memsz;
	userptr_t buffer;
	int rc;

	/* Verify and prepare segment */
	offset = ( hdr->offset - hdr->mb.header_addr + hdr->mb.load_addr );
	filesz = ( hdr->mb.load_end_addr - hdr->mb.load_addr );
	memsz = ( hdr->mb.bss_end_addr - hdr->mb.load_addr );
	buffer = phys_to_user ( hdr->mb.load_addr );
	if ( ( rc = prep_segment ( buffer, filesz, memsz ) ) != 0 ) {
		DBG ( "Multiboot could not prepare segment: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	copy_user ( buffer, 0, image->data, offset, filesz );

	/* Record execution entry point */
	image->entry = hdr->mb.entry_addr;
	image->execute = multiboot_execute;

	return 0;
}

/**
 * Load ELF multiboot image into memory
 *
 * @v image		Multiboot file
 * @ret rc		Return status code
 */
static int multiboot_load_elf ( struct image *image ) {
	int rc;

	/* Load ELF image*/
	if ( ( rc = elf_load ( image ) ) != 0 ) {
		DBG ( "Multiboot ELF image failed to load: %s\n",
		      strerror ( rc ) );
		/* We must translate "not an ELF image" (i.e. ENOEXEC)
		 * into "invalid multiboot image", to avoid screwing
		 * up the image probing logic.
		 */
		if ( rc == -ENOEXEC ) {
			return -ENOTSUP;
		} else {
			return rc;
		}
	}

	/* Capture execution method */
	if ( image->execute )
		image->execute = multiboot_execute;

	return 0;
}

/**
 * Load multiboot image into memory
 *
 * @v image		Multiboot file
 * @ret rc		Return status code
 */
int multiboot_load ( struct image *image ) {
	struct multiboot_header_info hdr;
	int rc;

	/* Locate multiboot header, if present */
	if ( ( rc = multiboot_find_header ( image, &hdr ) ) != 0 ) {
		DBG ( "No multiboot header\n" );
		return rc;
	}
	DBG ( "Found multiboot header with flags %08lx\n", hdr.mb.flags );

	/* Abort if we detect flags that we cannot support */
	if ( hdr.mb.flags & MB_UNSUPPORTED_FLAGS ) {
		DBG ( "Multiboot flags %08lx not supported\n",
		      ( hdr.mb.flags & MB_UNSUPPORTED_FLAGS ) );
		return -ENOTSUP;
	}

	/* Load the actual image */
	if ( hdr.mb.flags & MB_FLAG_RAW ) {
		if ( ( rc = multiboot_load_raw ( image, &hdr ) ) != 0 )
			return rc;
	} else {
		if ( ( rc = multiboot_load_elf ( image ) ) != 0 )
			return rc;
	}

	return 0;
}

/** Multiboot image type */
struct image_type multiboot_image_type __image_type = {
	.name = "Multiboot",
	.load = multiboot_load,
};
