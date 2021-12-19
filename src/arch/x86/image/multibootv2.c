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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bits/stdint.h"
FILE_LICENCE( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * Multiboot image format
 *
 */

#include <assert.h>
#include <errno.h>
#include <ipxe/elf.h>
#include <ipxe/features.h>
#include <ipxe/image.h>
#include <ipxe/init.h>
#include <ipxe/io.h>
#include <ipxe/segment.h>
#include <ipxe/uaccess.h>
#include <ipxe/uri.h>
#include <ipxe/version.h>
#include <multibootv2.h>
#include <realmode.h>
#include <stdio.h>

FEATURE( FEATURE_IMAGE, "MBOOT2", DHCP_EB_FEATURE_MULTIBOOT, 1 );

/**
 * Maximum number of header tags
 *
 * To prevent a deadlock we look for at max n header tags before aborting
 * the spec only defines 6 and there should be no duplicates
 */
#define MAX_HEADER_TAGS 14

/** A multiboot header descriptor */
struct multiboot_header_info {
	/** The actual multiboot header */
	struct multiboot_header mb;
	/** Useful information extracted from header tags */
	struct tags {
		bool need_page_align;
		uint32_t header_addr;
		uint32_t load_addr;
		uint32_t load_end_addr;
		uint32_t bss_end_addr;
		uint32_t entry_addr;
	} tags;

	/** Offset of header within the multiboot image */
	size_t offset;
};

/** Multiboot tags buffer **/
#define BOOT_INFO_BUFFER_SIZE 0x1000
static uint8_t __bss16_array( mbinfo, [BOOT_INFO_BUFFER_SIZE] );
#define mbinfo __use_data16( mbinfo )

/**
 * Pads value to alignement
 * @v value			Value to be aligned
 * @v alignment		Alignment to be adhered
 * @ret padded_val	Padded value
 */
static uint32_t pad_toX( uint32_t value, uint32_t alignment ) {
	uint32_t mod = value % alignment;
	if ( mod == 0 ) {
		return value;
	}

	return value + ( alignment - mod );
}

/**
 * Pads value to an 8 byte alignment
 * @v value		Value to be aligned
 * @ret padded_val	Padded value
 */
static uint32_t pad8( uint32_t value ) { return pad_toX( value, 8 ); }

/**
 * Find end of mbinfo list and creates a new tag
 *
 * @v curr_tag_local_ptr	Pointer Pointer pointing to a valid tag header in
 * mbinfo list
 * @v new_tag_size			Size of tag to be created
 * @v tag_type				Tag type to be set in header
 * @ret rc					Return status code
 */
static int add_tag_entry( uint8_t **curr_tag_local_ptr, uint32_t new_tag_size,
						  uint32_t tag_type ) {
	/** The first two bytes of the boot info struct aren't a tag and contain
	 * size information */
	struct multiboot_bootinfo_start *start_tag = (void *)&mbinfo;
	uint32 i = 0;

	/** First time add 8 bytes to total_size and index to jump over
	 * multiboot_start_tag with padding */
	if ( start_tag->total_size == 0 ) {
		start_tag->total_size = 8;
		i = 8;
	}

	/** Tags have to be 8 bytes aligned so we pad the tag size */
	uint32_t padded_tag_size = pad8( new_tag_size );

	/** Check that adding a new tag doesn't exceed the boot info buffer */
	if ( padded_tag_size + start_tag->total_size > BOOT_INFO_BUFFER_SIZE ) {
		DBG( "Padded tag size %d would exceed boot info buffer\n",
			 padded_tag_size );
		return -ENOBUFS;
	}

	uint8_t *curr_data_ptr = *curr_tag_local_ptr;

	/** Lineary search over the tags to find the end of the list marked by a
	 * tag type of 0 */
	while ( i < BOOT_INFO_BUFFER_SIZE ) {
		struct multiboot_bootinfo_header *tag = (void *)&curr_data_ptr[i];

		// Make sure that tag is 8 bytes aligned
		if ( (uint32_t)tag % 8 != 0 ) {
			DBG( "Index tag %d is not 8 bytes aligned 0x%lx\n", i,
				 virt_to_phys( tag ) );
			return -EINVAL;
		}

		// If we found a zeroed tag header its free
		if ( tag->type == 0 && tag->size == 0 ) {
			// We add the new tag size to the total size of our boot
			// info structure
			start_tag->total_size += padded_tag_size;

			// We increment the pointer behind the pointer to point to
			// current new tag
			*curr_tag_local_ptr += i;

			if ( *curr_tag_local_ptr + padded_tag_size !=
				 (uint8_t *)&mbinfo + start_tag->total_size ) {
				DBG( "total_size is incorrect\n" );
				return -EINVAL;
			}

			tag->size = padded_tag_size;
			tag->type = tag_type;

			DBG( "tag->type %d tag->size %d \n", tag->type, tag->size );
			DBG( "tag space from 0x%lx - 0x%lx\n",
				 virt_to_phys( *curr_tag_local_ptr ),
				 virt_to_phys( *curr_tag_local_ptr + padded_tag_size ) );
			return 0;
		} else {
			// Found tag has invalid size of zero
			if ( tag->size == 0 ) {
				DBG( "Found tag with invalid size of zero at addr "
					 "0x%lx\n",
					 virt_to_phys( tag ) );
				DBG( "tag hex: 0x%llx\n", *(uint64_t *)tag );
				return -EINVAL;
			}

			i += tag->size;
		}
	}

	// End of buffer reached. This should never happen
	DBG( "End of buffer reached. i = %d\n", i );
	return -EINVAL;
}

/**
 * Append string to multiboot tag. Only use on last element in boot info list
 *
 * @v tag_ptr	Pointer to last element in boot info list
 * @v offset	Offset where to append string to
 * @v data		Data to be appended to tag
 * @v data_len  Length of data
 * @ret rc		Return status code
 */
static int multiboot_append_data( uint8_t *tag_ptr, uint32_t offset, void *data,
								  size_t data_len ) {
	struct multiboot_bootinfo_header *tag = (void *)tag_ptr;
	struct multiboot_bootinfo_start *start_tag = (void *)&mbinfo;
	size_t total_padded_len = pad8( data_len + tag->size );
	size_t data_len_padded = total_padded_len - tag->size;

	// Check that appending a string doesn't exceed buffer
	if ( total_padded_len + start_tag->total_size > BOOT_INFO_BUFFER_SIZE ) {
		DBG( "Appending data  with len %u would exceed boot info buffer\n",
			 data_len );
		return -ENOBUFS;
	}

	start_tag->total_size += data_len_padded;
	tag->size = total_padded_len;

	memcpy( (char *)tag_ptr + offset, data, data_len );

	return 0;
}

/**
 * Build multiboot memory map
 *
 * @v image		Multiboot image
 * @v mbinfo	Pointer to mbinfo buffer from which to start
 * searching
 * @ret rc		Return status code
 */
static int multiboot_build_memmap( struct image *image, uint8_t **tag_ptr ) {
	struct memory_map memmap;
	unsigned int i;
	int rc;

	/* Get memory map */
	get_memmap( &memmap );

	// Add basic memory information tag
	if ( ( rc = add_tag_entry( tag_ptr,
							   sizeof( struct multiboot_memory_info_tag ),
							   MULTIBOOT_TAG_TYPE_BASIC_MEMINFO ) ) != 0 ) {
		DBGC( image,
			  "MULTIBOOT2 %p failed to add memory information tag. Code: %d",
			  image, rc );
		return rc;
	}
	struct multiboot_memory_info_tag *mem_info_tag = (void *)*tag_ptr;

	// Padd mmap entry size
	uint32_t entry_size_padded =
		pad8( sizeof( struct multiboot_memory_map_entry ) );

	// Calculate total mmap tag size with all entries
	uint32_t mem_tag_size = sizeof( struct multiboot_memory_map_tag ) +
							entry_size_padded * memmap.count;

	// Request mmap tag and write tag data
	if ( ( rc = add_tag_entry( tag_ptr, mem_tag_size,
							   MULTIBOOT_TAG_TYPE_MMAP ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p failed to add memory map tag. Code: %d",
			  image, rc );
		return rc;
	}
	struct multiboot_memory_map_tag *memmap_tag = (void *)*tag_ptr;
	memmap_tag->entry_size = entry_size_padded;
	memmap_tag->entry_version = 0;

	/* Translate bios memory map into multiboot format.
	 * Every entry in memmap array is a usable memory region
	 */
	for ( i = 0; i < memmap.count; i++ ) {
		// Calculate next map entry address
		uint8_t *new_tag_addr =
			(uint8_t *)memmap_tag + // We start by pointing to (struct
									// multiboot_memory_map_tag*)
			+sizeof(
				struct multiboot_memory_map_tag ) // We jump over struct
												  // multiboot_memory_map_tag
			+ memmap_tag->entry_size *
				  i; // We use current index to point to empty entry

		struct multiboot_memory_map_entry *new_tag = (void *)new_tag_addr;

		// Transfer data from ipxe memmap to memmap tag entry
		new_tag->base_addr = memmap.regions[i].start;
		new_tag->length = ( memmap.regions[i].end - memmap.regions[i].start );
		new_tag->type = MBMEM_RAM;

		DBGC( image,
			  "MULTIBOOT2 %d: base addr: 0x%llx length: 0x%llx "
			  "mem_location: "
			  "0x%lx\n",
			  i, new_tag->base_addr, new_tag->length, virt_to_phys( new_tag ) );

		// Update struct multiboot_memory_info_tag
		if ( memmap.regions[i].start == 0 )
			mem_info_tag->mem_lower = ( memmap.regions[i].end / 1024 );
		if ( memmap.regions[i].start == 0x100000 )
			mem_info_tag->mem_upper =
				( ( memmap.regions[i].end - 0x100000 ) / 1024 );
	}

	return 0;
}

/**
 * Add command line in base memory
 *
 * @v image		Image
 * @v tag_ptr   Pointer to a tag header
 * @v offset	Offset from tag_ptr to copy string to
 * @ret rc		Return status code
 */
static int multiboot_add_cmdline( struct image *image, void *tag_ptr,
								  uint32_t offset ) {
	char buf[512];
	memset( buf, 0, sizeof( buf ) );
	size_t len;
	size_t remaining = sizeof( buf );
	int rc;

	/* Copy image URI to base memory buffer as start of command line */
	len = ( format_uri( image->uri, buf, sizeof( buf ) - 1 ) + 1 /* NUL */ );
	remaining -= len;

	/* Copy command line to base memory buffer, if present */
	if ( image->cmdline ) {
		len--;		 // Overwrite NULL
		remaining++; // Overwrite NULL
		len += ( snprintf( buf + len, remaining, " %s", image->cmdline ) +
				 1 /* NUL */ );
	}

	if ( ( rc = multiboot_append_data( tag_ptr, offset, buf, len ) ) != 0 ) {
		DBGC( image, "MULTIBOOTV2 failed to append string\n" );
		return rc;
	}

	return 0;
}

/**
 * Add multiboot modules
 *
 * @v image		Multiboot image
 * @v start		Start address for modules
 * @v mbinfo		Pointer to mbinfo buffer from which to start
 * searching
 * @ret rc		Return status code
 */
static int multiboot_add_modules( struct image *image, physaddr_t start,
								  uint8_t **tag_ptr ) {
	struct image *module_image;
	int rc;

	/* Add each image as a multiboot module */
	for_each_image( module_image ) {
		/* Do not include kernel image itself as a module */
		if ( module_image == image )
			continue;

		if ( ( rc = add_tag_entry( tag_ptr,
								   sizeof( struct multiboot_module_tag ),
								   MULTIBOOT_TAG_TYPE_MODULE ) ) != 0 ) {
			DBGC( image,
				  "MULTIBOOT2 %p failed to add module to tag list. Code: %d",
				  image, rc );

			return rc;
		}

		/* Page-align the module */
		start = ( ( start + 0xfff ) & ~0xfff );

		/* Prepare segment */
		if ( ( rc = prep_segment( phys_to_user( start ), module_image->len,
								  module_image->len ) ) != 0 ) {
			DBGC( image,
				  "MULTIBOOT2 %p could not prepare module "
				  "%s: %s\n",
				  image, module_image->name, strerror( rc ) );
			return rc;
		}

		/* Copy module */
		memcpy_user( phys_to_user( start ), 0, module_image->data, 0,
					 module_image->len );

		/* Add module to tag list */
		struct multiboot_module_tag *new_tag = (void *)*tag_ptr;

		new_tag->mod_start = start;
		new_tag->mod_end = ( start + module_image->len );
		if ( ( rc = multiboot_add_cmdline(
				   module_image, new_tag,
				   sizeof( struct multiboot_module_tag ) ) ) != 0 ) {
			return rc;
		}
		DBGC( image, "MULTIBOOT2 %p module %s is [%x,%x)\n", image,
			  module_image->name, new_tag->mod_start, new_tag->mod_end );
		start += module_image->len;
	}

	return 0;
}

/**
 * Parse multiboot header tags
 * @v hdr_tags_begin   Pointer to start of header tags
 * @v hdr		       Multiboot header descriptor to fill in
 * @ret rc		       Return status code
 */
static int multiboot_parse_header_tags( struct image *image,
										struct multiboot_header_info *hdr ) {
	struct multiboot_header_tag tag;
	/* Copy first header tag */
	copy_from_user( &tag, image->data,
					hdr->offset + sizeof( struct multiboot_header ),
					sizeof( tag ) );

	for ( int i = 0; i < MAX_HEADER_TAGS; i++ ) {
		DBGC( image, "MULTIBOOT2 %p tag type: %x flags: %x size: %x\n", image,
			  tag.type, tag.flags, tag.size );

		switch ( tag.type ) {
		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
			hdr->tags.need_page_align = true;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS:
			hdr->tags.entry_addr = tag.entry_tag.entry_address;
			break;
		case MULTIBOOT_HEADER_TAG_ADDRESS:
			hdr->tags.header_addr = tag.address_tag.header_addr;
			hdr->tags.bss_end_addr = tag.address_tag.bss_end_addr;
			hdr->tags.load_addr = tag.address_tag.load_addr;
			hdr->tags.load_end_addr = tag.address_tag.load_end_addr;
			break;
		case MULTIBOOT_HEADER_TAG_END:
			if ( tag.size != 8 ) {
				DBGC( image,
					  "MULTIBOOT2 %p header end tag has to have "
					  "size 8 is "
					  "however %d\n",
					  image, tag.size );
				return -EINVAL;
			}

			return 0;
		default:
			if ( tag.flags != 0 ) {
				DBGC( image,
					  "MULTIBOOT2 %p header has unsupported "
					  "header tag %#x that "
					  "is "
					  "required\n",
					  image, tag.type );
				return -ENOTSUP;
			}
			break;
		}

		if ( hdr->offset + tag.size >= image->len ) {
			DBGC( image,
				  "MULTIBOOT2 %p header has invalid size in tag, this "
				  "exceeds "
				  "the image."
				  "Tag size: %x\n",
				  image, tag.size );
			return -EINVAL;
		}
		/* Copy next header tag */
		copy_from_user( &tag, image->data, hdr->offset + tag.size,
						sizeof( tag ) );
	}

	DBGC( image, "MULTIBOOT2 %p header has no end tag\n", image );
	return -EINVAL;
}

/**
 * Find multiboot header
 *
 * @v image		Multiboot file
 * @v hdr		Multiboot header descriptor to fill in
 * @ret rc		Return status code
 */
static int multiboot_find_header( struct image *image,
								  struct multiboot_header_info *hdr ) {
	uint32_t buf[64];
	size_t offset;
	unsigned int buf_idx;
	uint32_t checksum;

	/* Scan through first 8.5kB of image file 256 bytes at a time.
	 * (Use the buffering to avoid the overhead of a
	 * copy_from_user() for every dword.)
	 */
	for ( offset = 0; offset < 0x2200; offset += sizeof( buf[0] ) ) {
		/* Check for end of image */
		if ( offset > image->len ) {
			break;
		}
		/* Refill buffer if applicable */
		buf_idx = ( ( offset % sizeof( buf ) ) / sizeof( buf[0] ) );
		if ( buf_idx == 0 ) {
			copy_from_user( buf, image->data, offset, sizeof( buf ) );
		}
		/* Check signature */
		if ( buf[buf_idx] != MULTIBOOT_HEADER_MAGIC ) {
			continue;
		}

		/* Copy header and verify checksum */
		copy_from_user( &hdr->mb, image->data, offset, sizeof( hdr->mb ) );
		checksum = ( hdr->mb.magic + hdr->mb.architecture +
					 hdr->mb.header_length + hdr->mb.checksum );

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
 * @ret entry		Entry point
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
static int multiboot_load_raw( struct image *image,
							   struct multiboot_header_info *hdr,
							   physaddr_t *entry, physaddr_t *max ) {
	size_t offset;
	size_t filesz;
	size_t memsz;
	userptr_t buffer;
	int rc;

	/* Sanity check */
	if ( hdr->tags.entry_addr == 0 ) {
		DBGC( image,
			  "MULTIBOOT2 %p raw image does not have needed entry addr "
			  "header "
			  "tag\n",
			  image );
		return -EINVAL;
	}

	/* Verify and prepare segment */
	offset = ( hdr->offset - hdr->tags.header_addr + hdr->tags.load_addr );
	filesz = ( hdr->tags.load_end_addr
				   ? ( hdr->tags.load_end_addr - hdr->tags.load_addr )
				   : ( image->len - offset ) );
	memsz = ( hdr->tags.bss_end_addr
				  ? ( hdr->tags.bss_end_addr - hdr->tags.load_addr )
				  : filesz );
	buffer = phys_to_user( hdr->tags.load_addr );
	if ( ( rc = prep_segment( buffer, filesz, memsz ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p could not prepare segment: %s\n", image,
			  strerror( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy_user( buffer, 0, image->data, offset, filesz );

	/* Record execution entry point and maximum used address */
	*entry = hdr->tags.entry_addr;
	*max = ( hdr->tags.load_addr + memsz );

	return 0;
}

/**
 * Load ELF multiboot image into memory
 *
 * @v image		Multiboot file
 * @ret entry		Entry point
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
static int multiboot_load_elf( struct image *image, physaddr_t *entry,
							   physaddr_t *max ) {
	int rc;

	/* Load ELF image*/
	if ( ( rc = elf_load( image, entry, max ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p ELF image failed to load: %s\n", image,
			  strerror( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Execute multiboot image
 *
 * @v image		Multiboot image
 * @ret rc		Return status code
 */
static int multiboot_exec( struct image *image ) {
	struct multiboot_header_info hdr;
	memset( &hdr.tags, 0, sizeof( struct tags ) );
	physaddr_t entry;
	physaddr_t max;
	int rc;

	DBGC( image, "MULTIBOOT2 %p trying to find header...\n", image );
	/* Locate multiboot header, if present */
	if ( ( rc = multiboot_find_header( image, &hdr ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p has no multiboot header\n", image );
		return rc;
	}

	if ( ( rc = multiboot_parse_header_tags( image, &hdr ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p has invalid multiboot header tags\n",
			  image );
		return rc;
	}

	/* There is technically a bit MB_FLAG_RAW to indicate whether
	 * this is an ELF or a raw image.  In practice, grub will use
	 * the ELF header if present, and Solaris relies on this
	 * behaviour.
	 */
	if ( ( ( rc = multiboot_load_elf( image, &entry, &max ) ) != 0 ) &&
		 ( ( rc = multiboot_load_raw( image, &hdr, &entry, &max ) ) != 0 ) )
		return rc;

	/* Populate multiboot information structure */
	memset( &mbinfo, 0, sizeof( mbinfo ) );

	// Add command line tag to boot info structure
	uint8_t *tag_ptr = &mbinfo[0];

	if ( ( rc =
			   add_tag_entry( &tag_ptr, sizeof( struct multiboot_cmd_line_tag ),
							  MULTIBOOT_TAG_TYPE_CMDLINE ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p failed to add command line tag. Code: %d\n",
			  image, rc );
		return rc;
	}
	{
		struct multiboot_cmd_line_tag *new_tag = (void *)tag_ptr;
		if ( ( ( rc = multiboot_add_cmdline(
					 image, new_tag,
					 sizeof( struct multiboot_cmd_line_tag ) ) ) ) ) {
			return rc;
		}
	}

	// Add boot loader name tag
	if ( ( ( rc = add_tag_entry( &tag_ptr,
								 sizeof( struct multiboot_bootloader_name_tag ),
								 MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME ) ) ) ) {
		DBGC( image,
			  "MULTIBOOT2 %p failed to add boot loader name tag. Code: %d\n",
			  image, rc );

		return rc;
	}
	{
		char bootloader_name[32];
		memset( bootloader_name, 0, sizeof( bootloader_name ) );
		int len = snprintf( bootloader_name, sizeof( bootloader_name ) - 1,
							"iPXE %s", product_version );
		len++; // Make sure string is NULL terminated

		if ( ( rc = multiboot_append_data(
				   tag_ptr, sizeof( struct multiboot_bootloader_name_tag ),
				   bootloader_name, len ) ) != 0 ) {
			return rc;
		}
	}

	if ( ( rc = multiboot_add_modules( image, max, &tag_ptr ) ) != 0 )
		return rc;

	/* Multiboot images may not return and have no callback
	 * interface, so shut everything down prior to booting the OS.
	 */
	shutdown_boot();

	/* Build memory map after unhiding bootloader memory regions as part of
	 * shutting everything down.
	 */
	multiboot_build_memmap( image, &tag_ptr );

	// Add end tag
	if ( ( rc = add_tag_entry( &tag_ptr,
							   sizeof( struct multiboot_bootinfo_header ),
							   MULTIBOOT_TAG_TYPE_END ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p failed to add end tag. Code: %d\n", image,
			  rc );

		return rc;
	}

	/* Jump to OS with flat physical addressing */
	DBGC( image, "MULTIBOOT2 %p starting execution at %lx\n", image, entry );
	__asm__ __volatile__( PHYS_CODE( "pushl %%ebp\n\t"
									 "call *%%edi\n\t"
									 "popl %%ebp\n\t" )
						  :
						  : "a"( MULTIBOOT_BOOTLOADER_MAGIC ),
							"b"( virt_to_phys( &mbinfo ) ), "D"( entry )
						  : "ecx", "edx", "esi", "memory" );

	DBGC( image, "MULTIBOOT2 %p returned\n", image );

	/* It isn't safe to continue after calling shutdown() */
	while ( 1 ) {
	}

	return -ECANCELED; /* -EIMPOSSIBLE, anyone? */
}

/**
 * Probe multiboot image
 *
 * @v image		Multiboot file
 * @ret rc		Return status code
 */
static int multiboot_probe( struct image *image ) {
	struct multiboot_header_info hdr;
	int rc;

	/* Locate multiboot header, if present */
	if ( ( rc = multiboot_find_header( image, &hdr ) ) != 0 ) {
		DBGC( image, "MULTIBOOT2 %p has no multiboot header\n", image );
		return rc;
	}
	DBGC( image, "MULTIBOOT2 %p found header \n", image );

	return 0;
}

/** Multiboot image type */
struct image_type multibootv2_image_type __image_type( PROBE_MULTIBOOTV2 ) = {
	.name = "Multibootv2",
	.probe = multiboot_probe,
	.exec = multiboot_exec,
};
