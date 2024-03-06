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
#include <landing_zone.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/segment.h>
#include <ipxe/io.h>
#include <ipxe/elf.h>
#include <ipxe/init.h>
#include <ipxe/features.h>
#include <ipxe/umalloc.h>
#include <ipxe/uri.h>
#include <ipxe/version.h>
#ifdef EFIAPI
#include <ipxe/efi/efi.h>
#endif

FEATURE ( FEATURE_IMAGE, "MBOOT2", DHCP_EB_FEATURE_MULTIBOOT2, 1 );

/**
 * Maximum multiboot2 boot information size
 */
#define MB_MAX_BOOTINFO_SIZE 4096

/** Multiboot2 boot information buffer */
static union {
	uint64_t align;
	char bib[MB_MAX_BOOTINFO_SIZE];
} mb2_bib;

/** A multiboot2 header descriptor */
struct multiboot2_header_info {
	/** The actual multiboot2 header */
	struct multiboot_header mb;
	/** Offset of header within the multiboot2 image */
	size_t offset;
};

/**
 * Find multiboot2 header
 *
 * @v image		Multiboot file
 * @v hdr		Multiboot header descriptor to fill in
 * @ret rc		Return status code
 */
static int multiboot2_find_header ( struct image *image,
				   struct multiboot2_header_info *hdr ) {
	uint32_t buf[64];
	size_t offset;
	unsigned int buf_idx;
	uint32_t checksum;

	/* Scan through first MULTIBOOT_SEARCH of image file 256 bytes at a time.
	 * (Use the buffering to avoid the overhead of a
	 * copy_from_user() for every dword.)
	 */
	for ( offset = 0 ; offset < MULTIBOOT_SEARCH ; offset += sizeof ( buf[0] ) ) {
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
		if ( buf[buf_idx] != MULTIBOOT2_HEADER_MAGIC )
			continue;
		/* Copy header and verify checksum */
		copy_from_user ( &hdr->mb, image->data, offset,
				 sizeof ( hdr->mb ) );
		checksum = ( hdr->mb.magic + hdr->mb.architecture + hdr->mb.header_length +
				 hdr->mb.checksum );
		if ( checksum != 0 )
			continue;

		/* Make sure that the multiboot architecture is x86 */
		if (hdr->mb.architecture != MULTIBOOT_ARCHITECTURE_I386) {
			return -ENOEXEC;
		}

		/* Record offset of multiboot header and return */
		hdr->offset = offset;
		return 0;
	}

	/* No multiboot header found */
	return -ENOEXEC;
}

struct multiboot2_tags {
	int module_align;
	int boot_services;

	int entry_addr_valid;
	int entry_addr_efi32_valid;
	int entry_addr_efi64_valid;
	int relocatable_valid;

	uint32_t entry_addr;
	uint32_t entry_addr_efi32;
	uint32_t entry_addr_efi64;
	uint32_t reloc_min_addr;
	uint32_t reloc_max_addr;
	uint32_t reloc_align;
	uint32_t reloc_preference;
};

static int multiboot2_validate_inforeq ( struct image *image, size_t offset, size_t num_reqs ) {
	uint32_t inforeq;

	while (num_reqs) {
		copy_from_user ( &inforeq, image->data, offset, sizeof ( inforeq ) );
		offset += sizeof(inforeq);
		num_reqs--;

		switch (inforeq) {
		case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
		case MULTIBOOT_TAG_TYPE_MMAP:
			continue;

		default:
			return -ENOTSUP;
		}
	}

	return 0;
}

static int multiboot2_validate_tags ( struct image *image, struct multiboot2_header_info *hdr,
	   struct multiboot2_tags *tags ) {
	size_t offset = hdr->offset + sizeof(struct multiboot_header);
	size_t end_offset = offset + hdr->mb.header_length;
	struct multiboot_header_tag tag;

	/* Clear out the multiboot2 tags structure */
	memset(tags, 0, sizeof(*tags));

	while (offset < end_offset) {
		copy_from_user ( &tag, image->data, offset, sizeof ( tag ) );

		DBGC ( image, "MULTIBOOT2 %p (offset: %d) TAG type: %x flags: %x size: %d\n", image,
				(int)(offset - hdr->offset), tag.type, tag.flags, tag.size );

		if (tag.type == MULTIBOOT_HEADER_TAG_END) {
			DBGC ( image, "MULTIBOOT2 %p tag end\n", image );
			return 0;
		}

		switch (tag.type) {
		case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
		{
			size_t num_inforeqs;

			DBGC ( image, "MULTIBOOT2 %p has an information request tag\n",
				   image );

			num_inforeqs = (tag.size - sizeof(tag)) / sizeof(uint32_t);

			if (multiboot2_validate_inforeq ( image, offset + sizeof(tag), num_inforeqs ) != 0) {
				DBGC ( image, "MULTIBOOT2 %p cannot support all information request tags\n",
					   image );
				return -ENOTSUP;
			}

			break;
		}
		case MULTIBOOT_HEADER_TAG_ADDRESS:
			DBGC ( image, "MULTIBOOT2 %p has an address tag\n",
				   image );

			if ((tag.flags & MULTIBOOT_HEADER_TAG_OPTIONAL) != MULTIBOOT_HEADER_TAG_OPTIONAL)
				return -ENOTSUP;

			break;

		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS:
		{
			struct multiboot_header_tag_entry_address mb_tag = { 0 };
			copy_from_user ( &mb_tag, image->data, offset, tag.size );

			DBGC ( image, "MULTIBOOT2 %p has an entry address tag\n",
				   image );

			tags->entry_addr_valid = 1;
			tags->entry_addr = mb_tag.entry_addr;
			break;
		}
		case MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS:
			DBGC ( image, "MULTIBOOT2 %p has a console flags tag\n",
				   image );

			if ((tag.flags & MULTIBOOT_HEADER_TAG_OPTIONAL) != MULTIBOOT_HEADER_TAG_OPTIONAL)
				return -ENOTSUP;

			break;

		case MULTIBOOT_HEADER_TAG_FRAMEBUFFER:
			DBGC ( image, "MULTIBOOT2 %p has a framebuffer tag\n",
				   image );

			if ((tag.flags & MULTIBOOT_HEADER_TAG_OPTIONAL) != MULTIBOOT_HEADER_TAG_OPTIONAL)
				return -ENOTSUP;

			break;

		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
			DBGC ( image, "MULTIBOOT2 %p has a module align tag\n",
				   image );
			tags->module_align = 1;
			break;

		case MULTIBOOT_HEADER_TAG_EFI_BS:
			DBGC ( image, "MULTIBOOT2 %p has a boot services tag\n",
				   image );
			tags->boot_services = 1;
			break;

		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32:
		{
			struct multiboot_header_tag_entry_address mb_tag = { 0 };
			copy_from_user ( &mb_tag, image->data, offset, tag.size );

			DBGC ( image, "MULTIBOOT2 %p has an entry address EFI32 tag\n",
				   image );

			tags->entry_addr_efi32_valid = 1;
			tags->entry_addr_efi32 = mb_tag.entry_addr;
			break;
		}
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64:
		{
			struct multiboot_header_tag_entry_address mb_tag = { 0 };
			copy_from_user ( &mb_tag, image->data, offset, tag.size );

			DBGC ( image, "MULTIBOOT2 %p has an entry address EFI64 tag: %x\n",
				   image, mb_tag.entry_addr );

			tags->entry_addr_efi64_valid = 1;
			tags->entry_addr_efi64 = mb_tag.entry_addr;
			break;
		}
		case MULTIBOOT_HEADER_TAG_RELOCATABLE:
		{
			struct multiboot_header_tag_relocatable mb_tag = { 0 };
			copy_from_user ( &mb_tag, image->data, offset, tag.size );

			DBGC ( image, "MULTIBOOT2 %p has a relocatable tag\n",
				   image );

			tags->relocatable_valid = 1;
			tags->reloc_min_addr = mb_tag.min_addr;
			tags->reloc_max_addr = mb_tag.max_addr;
			tags->reloc_align = mb_tag.align;
			tags->reloc_preference = mb_tag.preference;
			break;
		}
		default:
			DBGC ( image, "MULTIBOOT2 %p unknown tag %x\n",
				   image, tag.type );
			return -ENOTSUP;
		}

		offset += tag.size + (MULTIBOOT_TAG_ALIGN - 1);
		offset = offset & ~(MULTIBOOT_TAG_ALIGN - 1);
	}

	/* If we did not get a MULTIBOOT_HEADER_TAG_END, fail out */
	DBGC ( image, "MULTIBOOT %p missing tag end\n", image );
	return -ENOTSUP;
}

/**
 * Add bootloader into bib
 */
static size_t multiboot2_add_bootloader ( struct image *image, size_t offset ) {
	struct multiboot_tag_string *bootloader = (struct multiboot_tag_string *)&mb2_bib.bib[offset];
	size_t remaining = MB_MAX_BOOTINFO_SIZE - offset - sizeof(*bootloader);
	size_t len;
	char *buf = bootloader->string;

	len = ( snprintf ( buf, remaining, "iPXE %s", product_version ) + 1 /* NUL */ );
	if ( len > remaining )
		len = remaining;

	DBGC ( image, "MULTIBOOT2 %p bootloader: %s\n", image, bootloader->string );

	bootloader->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
	bootloader->size = len + sizeof(*bootloader);
	return bootloader->size;
}

/**
 * Add command line into bib
 */
static size_t multiboot2_add_cmdline ( struct image *image, size_t offset ) {
	struct multiboot_tag_string *cmdline = (struct multiboot_tag_string *)&mb2_bib.bib[offset];
	size_t remaining = MB_MAX_BOOTINFO_SIZE - offset - sizeof(*cmdline);
	size_t len;
	char *buf = cmdline->string;

	cmdline->type = MULTIBOOT_TAG_TYPE_CMDLINE;
	cmdline->size = sizeof(*cmdline);

	/* Copy image URI to base memory buffer as start of command line */
	len = ( format_uri ( image->uri, buf, remaining ) + 1 /* NUL */ );
	if ( len > remaining )
		len = remaining;
	buf += len;
	remaining -= len;
	cmdline->size += len;

	/* Copy command line to base memory buffer, if present */
	if ( image->cmdline ) {
		buf--;
		cmdline->size--;
		remaining++;
		len = ( snprintf ( buf, remaining, " %s", image->cmdline ) + 1 /* NUL */ );
		if ( len > remaining )
			len = remaining;
	}

	DBGC ( image, "MULTIBOOT2 %p cmdline: %s\n", image, cmdline->string );

	cmdline->size += len;
	return cmdline->size;
}

/**
 * Load multiboot2 image into memory
 *
 * @v image		Multiboot file
 * @v hdr		Multiboot header descriptor
 * @ret entry		Entry point
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
static int multiboot2_load ( struct image *image, struct multiboot2_tags *tags,
				physaddr_t *load, physaddr_t *entry, physaddr_t *max ) {

	int rc;

	if ( ( rc = elf_load ( image, load, entry, max ) ) < 0 ) {
		DBGC ( image, "MULTIBOOT2 %p could not load elf image\n", image );
		return rc;
	}
#ifdef EFIAPI
	*entry = tags->entry_addr_efi64;
#else
	(void)tags;
#endif

	return rc;
}

static size_t adjust_tag_offset(size_t offset) {
	if ((offset & 7) != 0) {
		return ((offset + 8) & ~7);
	}
	return offset;
}

/**
 * Add multiboot modules
 */
static size_t multiboot2_add_modules ( struct image *image, size_t offset ) {
	struct image *module_image;
	struct multiboot_tag_module *module;
	char *buf;
	size_t remaining;
	size_t len;
	userptr_t memory;

	/* Add each image as a multiboot module */
	for_each_image ( module_image ) {

		/* Do not include kernel image itself as a module */
		if ( module_image == image )
			continue;

		memory = umalloc ( module_image->len );
		if ( memory == UNULL ) {
			DBGC ( image, "MULTIBOOT2 %p could not allocate %zd bytes.\n", module_image, module_image->len );
			return 0;
		}

		memcpy_user ( memory, 0, module_image->data, 0, module_image->len );

		/* Add module to list */
		module = (struct multiboot_tag_module *)&mb2_bib.bib[offset];
		module->type = MULTIBOOT_TAG_TYPE_MODULE;
		module->size = sizeof(*module);
		module->mod_start = user_to_phys ( memory, 0 );
		module->mod_end = user_to_phys ( memory, module_image->len );

		buf = module->cmdline;
		remaining = MB_MAX_BOOTINFO_SIZE - offset - sizeof(*module);

		/* Copy image URI to base memory buffer as start of command line */
		len = ( format_uri ( module_image->uri, buf, remaining ) + 1 /* NUL */ );
		if ( len > remaining )
			len = remaining;
		buf += len;
		remaining -= len;
		module->size += len;

		/* Copy command line to base memory buffer, if present */
		if ( module_image->cmdline ) {
			buf--;
			module->size--;
			remaining++;
			len = ( snprintf ( buf, remaining, " %s", module_image->cmdline ) + 1 /* NUL */ );
			if ( len > remaining )
				len = remaining;
			module->size += len;
		}

		offset += module->size;
		offset = adjust_tag_offset(offset);

		DBGC ( image, "MULTIBOOT2 %p module %s is [%x,%x): %s\n",
			   image, module_image->name, module->mod_start,
			   module->mod_end, module->cmdline );
	}

	return offset;
}

void multiboot2_boot(uint32_t *bib, uint32_t entry) {
/* This should probably test for x64 directly instead of UEFI */
#ifdef EFIAPI
	__asm__ __volatile__ ( "push %%rbp\n\t"
						   "call *%%rdi\n\t"
						   "pop %%rbp\n\t"
					   : : "a" ( MULTIBOOT2_BOOTLOADER_MAGIC ),
						   "b" ( bib ),
						   "D" ( entry )
						 : "rcx", "rdx", "rsi", "memory" );
#else
	__asm__ __volatile__ ( "push %%ebp\n\t"
						   "call *%%edi\n\t"
						   "pop %%ebp\n\t"
					   : : "a" ( MULTIBOOT2_BOOTLOADER_MAGIC ),
						   "b" ( bib ),
						   "D" ( entry )
						 : "ecx", "edx", "esi", "memory" );
#endif
}

/**
 * Execute multiboot2 image
 *
 * @v image		Multiboot image
 * @ret rc		Return status code
 */
static int multiboot2_exec ( struct image *image ) {
	struct multiboot2_header_info hdr;
	struct multiboot2_tags mb_tags;
	struct multiboot_tag *tag;
	struct multiboot_tag_load_base_addr *load_base_addr_tag;
#ifdef EFIAPI
	struct multiboot_tag_efi64 *tag_efi64;
#endif
	struct image *lz;
	uint32_t *total_size;
	uint32_t *reserved;
	physaddr_t load;
	physaddr_t entry;
	physaddr_t max;
	size_t offset;
	int rc;

	/* Locate multiboot2 header, if present */
	if ( ( rc = multiboot2_find_header ( image, &hdr ) ) != 0 ) {
		DBGC ( image, "MULTIBOOT2 %p has no multiboot header\n",
			   image );
		return rc;
	}

	/* Abort if we detect tags that we cannot support */
	if ( ( rc = multiboot2_validate_tags ( image, &hdr, &mb_tags ) ) != 0 ) {
		DBGC ( image, "MULTIBOOT2 %p contains unsupported tags\n",
			   image );
		return -ENOTSUP;
	}

	/* Attempt to load the image into memory of our choosing */
	if ( ( rc = multiboot2_load ( image, &mb_tags, &load, &entry, &max ) ) != 0) {
		DBGC ( image, "MULTIBOOT2 %p could not load\n", image );
		return rc;
	}

	if ( ( lz = find_image ( "landing_zone" ) ) != NULL ) {
		physaddr_t *args = (physaddr_t *) &mb2_bib;
		unregister_image ( image_get ( lz ) );

		max = ( max + LZ_ALIGN - 1 ) & ~( LZ_ALIGN - 1 );

		args[0] = entry;
		Elf_Ehdr *Ehdr = (Elf_Ehdr *) image->data;
		Elf_Phdr *Phdr = (Elf_Phdr *) ( image->data + Ehdr->e_phoff );
		args[1] = Phdr->p_filesz;
		lz_set ( lz, ( userptr_t ) &mb2_bib, phys_to_user ( max ),
		         LZ_PROTO_MULTIBOOT2 );
		/* Doesn't seem that max is used anywhere... Can LZ and kernel be
		 * overwritten by modules?
		 */
		max += SLB_SIZE;
	}

	/* Populate multiboot information structure */
	offset = 0;

	total_size = (uint32_t *)&mb2_bib.bib[offset];
	offset += sizeof(*total_size);

	reserved = (uint32_t *)&mb2_bib.bib[offset];
	offset += sizeof(*reserved);

	/* Clear out the reserved word */
	*reserved = 0;

	/* Add the load base address tag */
	load_base_addr_tag = (struct multiboot_tag_load_base_addr *)&mb2_bib.bib[offset];
	load_base_addr_tag->type = MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR;
	load_base_addr_tag->size = sizeof(*load_base_addr_tag);
	load_base_addr_tag->load_base_addr = load;
	offset += load_base_addr_tag->size;
	offset = adjust_tag_offset(offset);

#ifdef EFIAPI
	/* Add the EFI boot services not terminated tag */
	tag = (struct multiboot_tag *)&mb2_bib.bib[offset];
	tag->type = MULTIBOOT_TAG_TYPE_EFI_BS;
	tag->size = sizeof(*tag);
	offset += tag->size;
	offset = adjust_tag_offset(offset);

	/* Add the EFI 64-bit image handle pointer */
	tag_efi64 = (struct multiboot_tag_efi64 *)&mb2_bib.bib[offset];
	tag_efi64->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
	tag_efi64->size = sizeof(*tag_efi64);
	tag_efi64->pointer = (multiboot_uint64_t)efi_image_handle;
	offset += tag_efi64->size;
	offset = adjust_tag_offset(offset);

	/* Add the EFI 64-bit system table handle pointer */
	tag_efi64 = (struct multiboot_tag_efi64 *)&mb2_bib.bib[offset];
	tag_efi64->type = MULTIBOOT_TAG_TYPE_EFI64;
	tag_efi64->size = sizeof(*tag_efi64);
	tag_efi64->pointer = (multiboot_uint64_t)efi_systab;
	offset += tag_efi64->size;
	offset = adjust_tag_offset(offset);
#endif

	/* add the boot command line */
	offset += multiboot2_add_cmdline ( image, offset );
	offset = adjust_tag_offset(offset);

	/* add the bootloader */
	offset += multiboot2_add_bootloader ( image, offset );
	offset = adjust_tag_offset(offset);

	/* Add the modules */
	offset = multiboot2_add_modules ( image, offset );
	offset = adjust_tag_offset(offset);

	/* TODO: provide memory information */

	/* Terminate the tags */
	tag = (struct multiboot_tag *)&mb2_bib.bib[offset];
	tag->type = 0;
	tag->size = sizeof(*tag);
	offset += tag->size;

	*total_size = offset;

	DBGC ( image, "MULTIBOOT2 %p BIB is %d bytes\n", image, *total_size );

	/* Multiboot images may not return and have no callback
	 * interface, so shut everything down prior to booting the OS.
	 */
	shutdown_boot();

	if ( lz != NULL ) {
		register_image ( lz );
		image_put ( lz );

		return image_replace ( lz );
	}

	/* Jump to OS with flat physical addressing */
	DBGC ( image, "MULTIBOOT2 %p starting execution at %lx\n", image, entry );

	multiboot2_boot ( total_size, entry );
	DBGC ( image, "MULTIBOOT2 %p returned\n", image );

	/* It isn't safe to continue after calling shutdown() */
	while ( 1 ) {}

	return -ECANCELED;  /* -EIMPOSSIBLE, anyone? */
}

/**
 * Probe multiboot2 image
 *
 * @v image		Multiboot file
 * @ret rc		Return status code
 */
static int multiboot2_probe ( struct image *image ) {
	struct multiboot2_header_info hdr;
	int rc;

	/* Locate multiboot2 header, if present */
	if ( ( rc = multiboot2_find_header ( image, &hdr ) ) != 0 ) {
		DBGC ( image, "MULTIBOOT2 %p has no multiboot2 header\n",
			   image );
		return rc;
	}
	DBGC ( image, "MULTIBOOT2 %p found header with architecture %08x and header_length %d\n",
		   image, hdr.mb.architecture, hdr.mb.header_length );
	return 0;
}

/** Multiboot image type */
struct image_type multiboot2_image_type __image_type ( PROBE_MULTIBOOT2 ) = {
	.name = "Multiboot 2",
	.probe = multiboot2_probe,
	.exec = multiboot2_exec,
};
