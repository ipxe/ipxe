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
#include <assert.h>
#include <alloca.h>
#include <multiboot.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/memmap.h>
#include <gpxe/elf.h>

struct image_type multiboot_image_type __image_type;

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
 * Build multiboot memory map
 *
 * @v mbinfo		Multiboot information structure
 * @v mbmemmap		Multiboot memory map
 */
static void multiboot_build_memmap ( struct multiboot_info *mbinfo,
				     struct multiboot_memory_map *mbmemmap ) {
	struct memory_map memmap;
	unsigned int i;

	/* Get memory map */
	get_memmap ( &memmap );

	/* Translate into multiboot format */
	memset ( mbmemmap, 0, sizeof ( *mbmemmap ) );
	for ( i = 0 ; i < memmap.count ; i++ ) {
		mbmemmap[i].size = sizeof ( mbmemmap[i] );
		mbmemmap[i].base_addr = memmap.regions[i].start;
		mbmemmap[i].length = ( memmap.regions[i].end -
				       memmap.regions[i].start );
		mbmemmap[i].type = MBMEM_RAM;
		mbinfo->mmap_length += sizeof ( mbmemmap[i] );
		if ( memmap.regions[i].start == 0 )
			mbinfo->mem_lower = memmap.regions[i].end;
		if ( memmap.regions[i].start == 0x100000 )
			mbinfo->mem_upper = ( memmap.regions[i].end -
					      0x100000 );
	}
}

/**
 * Build multiboot module list
 *
 * @v image		Multiboot image
 * @v modules		Module list to fill, or NULL
 * @ret count		Number of modules
 */
static unsigned int
multiboot_build_module_list ( struct image *image,
			      struct multiboot_module *modules ) {
	struct image *module_image;
	struct multiboot_module *module;
	unsigned int count = 0;

	for_each_image ( module_image ) {

		/* Do not include kernel image itself as a module */
		if ( module_image == image )
			continue;
		module = &modules[count++];

		/* Populate module data structure, if applicable */
		if ( ! modules )
			continue;
		module->mod_start = user_to_phys ( module_image->data, 0 );
		module->mod_end = user_to_phys ( module_image->data,
						 module_image->len );
		if ( image->cmdline )
			module->string = virt_to_phys ( image->cmdline );

		/* We promise to page-align modules, so at least check */
		assert ( ( module->mod_start & 0xfff ) == 0 );
	}

	return count;
}

/**
 * Execute multiboot image
 *
 * @v image		Multiboot image
 * @ret rc		Return status code
 */
static int multiboot_exec ( struct image *image ) {
	static const char *bootloader_name = "gPXE " VERSION;
	struct multiboot_info mbinfo;
	struct multiboot_memory_map mbmemmap[MAX_MEMORY_REGIONS];
	struct multiboot_module *modules;
	unsigned int num_modules;

	/* Populate multiboot information structure */
	memset ( &mbinfo, 0, sizeof ( mbinfo ) );

	/* Set boot loader name */
	mbinfo.boot_loader_name = virt_to_phys ( bootloader_name );
	mbinfo.flags |= MBI_FLAG_LOADER;
	
	/* Build memory map */
	multiboot_build_memmap ( &mbinfo, mbmemmap );
	mbinfo.mmap_addr = virt_to_phys ( &mbmemmap[0].base_addr );
	mbinfo.flags |= ( MBI_FLAG_MEM | MBI_FLAG_MMAP );

	/* Set command line, if present */
	if ( image->cmdline ) {
		mbinfo.cmdline = virt_to_phys ( image->cmdline );
		mbinfo.flags |= MBI_FLAG_CMDLINE;
	}

	/* Construct module list */
	num_modules = multiboot_build_module_list ( image, NULL );
	modules = alloca ( num_modules * sizeof ( *modules ) );
	multiboot_build_module_list ( image, modules );
	mbinfo.mods_count = num_modules;
	mbinfo.mods_addr = virt_to_phys ( modules );
	mbinfo.flags |= MBI_FLAG_MODS;

	/* Jump to OS with flat physical addressing */
	__asm__ __volatile__ ( PHYS_CODE ( /* Preserve %ebp for alloca() */
					   "pushl %%ebp\n\t"
					   "call *%%edi\n\t"
					   "popl %%ebp\n\t" )
			       : : "a" ( MULTIBOOT_BOOTLOADER_MAGIC ),
			           "b" ( virt_to_phys ( &mbinfo ) ),
			           "D" ( image->entry )
			       : "ecx", "edx", "esi", "memory" );

	return -ECANCELED;
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
	memcpy_user ( buffer, 0, image->data, offset, filesz );

	/* Record execution entry point */
	image->entry = hdr->mb.entry_addr;

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
		return rc;
	}

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

	/* This is a multiboot image, valid or otherwise */
	if ( ! image->type )
		image->type = &multiboot_image_type;

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
	.exec = multiboot_exec,
};
