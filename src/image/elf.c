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
 * ELF image format
 *
 * A "pure" ELF image is not a bootable image.  There are various
 * bootable formats based upon ELF (e.g. Multiboot), which share
 * common ELF-related functionality.
 */

#include <errno.h>
#include <elf.h>
#include <gpxe/uaccess.h>
#include <gpxe/segment.h>
#include <gpxe/image.h>
#include <gpxe/elf.h>

typedef Elf32_Ehdr	Elf_Ehdr;
typedef Elf32_Phdr	Elf_Phdr;
typedef Elf32_Off	Elf_Off;

/**
 * Load ELF segment into memory
 *
 * @v image		ELF file
 * @v phdr		ELF program header
 * @ret rc		Return status code
 */
static int elf_load_segment ( struct image *image, Elf_Phdr *phdr ) {
	physaddr_t dest;
	userptr_t buffer;
	int rc;

	/* Do nothing for non-PT_LOAD segments */
	if ( phdr->p_type != PT_LOAD )
		return 0;

	/* Check segment lies within image */
	if ( ( phdr->p_offset + phdr->p_filesz ) > image->len ) {
		DBG ( "ELF segment outside ELF file\n" );
		return -ENOEXEC;
	}

	/* Find start address: use physical address for preference,
	 * fall back to virtual address if no physical address
	 * supplied.
	 */
	dest = phdr->p_paddr;
	if ( ! dest )
		dest = phdr->p_vaddr;
	if ( ! dest ) {
		DBG ( "ELF segment loads to physical address 0\n" );
		return -ENOEXEC;
	}
	buffer = phys_to_user ( dest );

	DBG ( "ELF loading segment [%x,%x) to [%x,%x,%x)\n",
	      phdr->p_offset, ( phdr->p_offset + phdr->p_filesz ),
	      phdr->p_paddr, ( phdr->p_paddr + phdr->p_filesz ),
	      ( phdr->p_paddr + phdr->p_memsz ) );

	/* Verify and prepare segment */
	if ( ( rc = prep_segment ( buffer, phdr->p_filesz,
				   phdr->p_memsz ) ) != 0 ) {
		DBG ( "ELF could not prepare segment: %s\n", strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy_user ( buffer, 0, image->data, phdr->p_offset, phdr->p_filesz );

	return 0;
}

/**
 * Load ELF image into memory
 *
 * @v image		ELF file
 * @ret rc		Return status code
 */
int elf_load ( struct image *image ) {
	Elf_Ehdr ehdr;
	Elf_Phdr phdr;
	Elf_Off phoff;
	unsigned int phnum;
	int rc;

	/* Image type must already have been set by caller */
	assert ( image->type != NULL );

	/* Read ELF header */
	copy_from_user ( &ehdr, image->data, 0, sizeof ( ehdr ) );
	if ( memcmp ( &ehdr.e_ident[EI_MAG0], ELFMAG, SELFMAG ) != 0 ) {
		DBG ( "Invalid ELF signature\n" );
		return -ENOEXEC;
	}

	/* Read ELF program headers */
	for ( phoff = ehdr.e_phoff , phnum = ehdr.e_phnum ; phnum ;
	      phoff += ehdr.e_phentsize, phnum-- ) {
		if ( phoff > image->len ) {
			DBG ( "ELF program header %d outside ELF image\n",
			      phnum );
			return -ENOEXEC;
		}
		copy_from_user ( &phdr, image->data, phoff, sizeof ( phdr ) );
		if ( ( rc = elf_load_segment ( image, &phdr ) ) != 0 )
			return rc;
	}

	/* Record execution entry point in image private data field */
	image->priv.phys = ehdr.e_entry;

	return 0;
}
