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

/**
 * @file
 *
 * El Torito bootable ISO image format
 *
 */

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <bootsector.h>
#include <int13.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/ramdisk.h>
#include <gpxe/init.h>

#define ISO9660_BLKSIZE 2048
#define ELTORITO_VOL_DESC_OFFSET ( 17 * ISO9660_BLKSIZE )

/** An El Torito Boot Record Volume Descriptor */
struct eltorito_vol_desc {
	/** Boot record indicator; must be 0 */
	uint8_t record_indicator;
	/** ISO-9660 identifier; must be "CD001" */
	uint8_t iso9660_id[5];
	/** Version, must be 1 */
	uint8_t version;
	/** Boot system indicator; must be "EL TORITO SPECIFICATION" */
	uint8_t system_indicator[32];
	/** Unused */
	uint8_t unused[32];
	/** Boot catalog sector */
	uint32_t sector;
} __attribute__ (( packed ));

/** An El Torito Boot Catalog Validation Entry */
struct eltorito_validation_entry {
	/** Header ID; must be 1 */
	uint8_t header_id;
	/** Platform ID
	 *
	 * 0 = 80x86
	 * 1 = PowerPC
	 * 2 = Mac
	 */
	uint8_t platform_id;
	/** Reserved */
	uint16_t reserved;
	/** ID string */
	uint8_t id_string[24];
	/** Checksum word */
	uint16_t checksum;
	/** Signature; must be 0xaa55 */
	uint16_t signature;
} __attribute__ (( packed ));

/** A bootable entry in the El Torito Boot Catalog */
struct eltorito_boot_entry {
	/** Boot indicator
	 *
	 * Must be @c ELTORITO_BOOTABLE for a bootable ISO image
	 */
	uint8_t indicator;
	/** Media type
	 *
	 */
	uint8_t media_type;
	/** Load segment */
	uint16_t load_segment;
	/** System type */
	uint8_t filesystem;
	/** Unused */
	uint8_t reserved_a;
	/** Sector count */
	uint16_t length;
	/** Starting sector */
	uint32_t start;
	/** Unused */
	uint8_t reserved_b[20];
} __attribute__ (( packed ));

/** Boot indicator for a bootable ISO image */
#define ELTORITO_BOOTABLE 0x88

/** El Torito media types */
enum eltorito_media_type {
	/** No emulation */
	ELTORITO_NO_EMULATION = 0,
};

struct image_type eltorito_image_type __image_type ( PROBE_NORMAL );

/**
 * Calculate 16-bit word checksum
 *
 * @v data		Data to checksum
 * @v len		Length (in bytes, must be even)
 * @ret sum		Checksum
 */
static unsigned int word_checksum ( void *data, size_t len ) {
	uint16_t *words;
	uint16_t sum = 0;

	for ( words = data ; len ; words++, len -= 2 ) {
		sum += *words;
	}
	return sum;
}

/**
 * Execute El Torito image
 *
 * @v image		El Torito image
 * @ret rc		Return status code
 */
static int eltorito_exec ( struct image *image ) {
	struct ramdisk ramdisk;
	struct int13_drive int13_drive;
	unsigned int load_segment = image->priv.ul;
	unsigned int load_offset = ( load_segment ? 0 : 0x7c00 );
	int rc;

	memset ( &ramdisk, 0, sizeof ( ramdisk ) );
	init_ramdisk ( &ramdisk, image->data, image->len, ISO9660_BLKSIZE );
	
	memset ( &int13_drive, 0, sizeof ( int13_drive ) );
	int13_drive.blockdev = &ramdisk.blockdev;
	register_int13_drive ( &int13_drive );

	if ( ( rc = call_bootsector ( load_segment, load_offset, 
				      int13_drive.drive ) ) != 0 ) {
		DBGC ( image, "ElTorito %p boot failed: %s\n",
		       image, strerror ( rc ) );
		goto err;
	}
	
	rc = -ECANCELED; /* -EIMPOSSIBLE */
 err:
	unregister_int13_drive ( &int13_drive );
	return rc;
}

/**
 * Read and verify El Torito Boot Record Volume Descriptor
 *
 * @v image		El Torito file
 * @ret catalog_offset	Offset of Boot Catalog
 * @ret rc		Return status code
 */
static int eltorito_read_voldesc ( struct image *image,
				   unsigned long *catalog_offset ) {
	static const struct eltorito_vol_desc vol_desc_signature = {
		.record_indicator = 0,
		.iso9660_id = "CD001",
		.version = 1,
		.system_indicator = "EL TORITO SPECIFICATION",
	};
	struct eltorito_vol_desc vol_desc;

	/* Sanity check */
	if ( image->len < ( ELTORITO_VOL_DESC_OFFSET + ISO9660_BLKSIZE ) ) {
		DBGC ( image, "ElTorito %p too short\n", image );
		return -ENOEXEC;
	}

	/* Read and verify Boot Record Volume Descriptor */
	copy_from_user ( &vol_desc, image->data, ELTORITO_VOL_DESC_OFFSET,
			 sizeof ( vol_desc ) );
	if ( memcmp ( &vol_desc, &vol_desc_signature,
		      offsetof ( typeof ( vol_desc ), sector ) ) != 0 ) {
		DBGC ( image, "ElTorito %p invalid Boot Record Volume "
		       "Descriptor\n", image );
		return -ENOEXEC;
	}
	*catalog_offset = ( vol_desc.sector * ISO9660_BLKSIZE );

	DBGC ( image, "ElTorito %p boot catalog at offset %#lx\n",
	       image, *catalog_offset );

	return 0;
}

/**
 * Read and verify El Torito Boot Catalog
 *
 * @v image		El Torito file
 * @v catalog_offset	Offset of Boot Catalog
 * @ret boot_entry	El Torito boot entry
 * @ret rc		Return status code
 */
static int eltorito_read_catalog ( struct image *image,
				   unsigned long catalog_offset,
				   struct eltorito_boot_entry *boot_entry ) {
	struct eltorito_validation_entry validation_entry;

	/* Sanity check */
	if ( image->len < ( catalog_offset + ISO9660_BLKSIZE ) ) {
		DBGC ( image, "ElTorito %p bad boot catalog offset %#lx\n",
		       image, catalog_offset );
		return -ENOEXEC;
	}

	/* Read and verify the Validation Entry of the Boot Catalog */
	copy_from_user ( &validation_entry, image->data, catalog_offset,
			 sizeof ( validation_entry ) );
	if ( word_checksum ( &validation_entry,
			     sizeof ( validation_entry ) ) != 0 ) {
		DBGC ( image, "ElTorito %p bad Validation Entry checksum\n",
		       image );
		return -ENOEXEC;
	}

	/* Read and verify the Initial/Default entry */
	copy_from_user ( boot_entry, image->data,
			 ( catalog_offset + sizeof ( validation_entry ) ),
			 sizeof ( *boot_entry ) );
	if ( boot_entry->indicator != ELTORITO_BOOTABLE ) {
		DBGC ( image, "ElTorito %p not bootable\n", image );
		return -ENOEXEC;
	}
	if ( boot_entry->media_type != ELTORITO_NO_EMULATION ) {
		DBGC ( image, "ElTorito %p cannot support media type %d\n",
		       image, boot_entry->media_type );
		return -ENOTSUP;
	}

	DBGC ( image, "ElTorito %p media type %d segment %04x\n",
	       image, boot_entry->media_type, boot_entry->load_segment );

	return 0;
}

/**
 * Load El Torito virtual disk image into memory
 *
 * @v image		El Torito file
 * @v boot_entry	El Torito boot entry
 * @ret rc		Return status code
 */
static int eltorito_load_disk ( struct image *image,
				struct eltorito_boot_entry *boot_entry ) {
	unsigned long start = ( boot_entry->start * ISO9660_BLKSIZE );
	unsigned long length = ( boot_entry->length * ISO9660_BLKSIZE );
	unsigned int load_segment;
	userptr_t buffer;
	int rc;

	/* Sanity check */
	if ( image->len < ( start + length ) ) {
		DBGC ( image, "ElTorito %p virtual disk lies outside image\n",
		       image );
		return -ENOEXEC;
	}
	DBGC ( image, "ElTorito %p virtual disk at %#lx+%#lx\n",
	       image, start, length );

	/* Calculate load address */
	load_segment = boot_entry->load_segment;
	buffer = real_to_user ( load_segment, ( load_segment ? 0 : 0x7c00 ) );

	/* Verify and prepare segment */
	if ( ( rc = prep_segment ( buffer, length, length ) ) != 0 ) {
		DBGC ( image, "ElTorito %p could not prepare segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy_user ( buffer, 0, image->data, start, length );

	return 0;
}

/**
 * Load El Torito image into memory
 *
 * @v image		El Torito file
 * @ret rc		Return status code
 */
static int eltorito_load ( struct image *image ) {
	struct eltorito_boot_entry boot_entry;
	unsigned long bootcat_offset;
	int rc;

	/* Read Boot Record Volume Descriptor, if present */
	if ( ( rc = eltorito_read_voldesc ( image, &bootcat_offset ) ) != 0 )
		return rc;

	/* This is an El Torito image, valid or otherwise */
	if ( ! image->type )
		image->type = &eltorito_image_type;

	/* Read Boot Catalog */
	if ( ( rc = eltorito_read_catalog ( image, bootcat_offset,
					    &boot_entry ) ) != 0 )
		return rc;

	/* Load Virtual Disk image */
	if ( ( rc = eltorito_load_disk ( image, &boot_entry ) ) != 0 )
		return rc;

	/* Record load segment in image private data field */
	image->priv.ul = boot_entry.load_segment;

	return 0;
}

/** El Torito image type */
struct image_type eltorito_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "El Torito",
	.load = eltorito_load,
	.exec = eltorito_exec,
};
