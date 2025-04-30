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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * ELF image format
 *
 * A "pure" ELF image is not a bootable image.  There are various
 * bootable formats based upon ELF (e.g. Multiboot), which share
 * common ELF-related functionality.
 */

#include <string.h>
#include <errno.h>
#include <elf.h>
#include <ipxe/segment.h>
#include <ipxe/image.h>
#include <ipxe/uaccess.h>
#include <ipxe/elf.h>

/**
 * Load ELF segment into memory
 *
 * @v image		ELF file
 * @v phdr		ELF program header
 * @v dest		Destination address
 * @ret rc		Return status code
 */
static int elf_load_segment ( struct image *image, const Elf_Phdr *phdr,
			      physaddr_t dest ) {
	void *buffer = phys_to_virt ( dest );
	int rc;

	DBGC ( image, "ELF %s loading segment [%x,%x) to [%lx,%lx,%lx)\n",
	       image->name, phdr->p_offset, ( phdr->p_offset + phdr->p_filesz ),
	       dest, ( dest + phdr->p_filesz ), ( dest + phdr->p_memsz ) );

	/* Verify and prepare segment */
	if ( ( rc = prep_segment ( buffer, phdr->p_filesz,
				   phdr->p_memsz ) ) != 0 ) {
		DBGC ( image, "ELF %s could not prepare segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy ( buffer, ( image->data + phdr->p_offset ), phdr->p_filesz );

	return 0;
}

/**
 * Process ELF segment
 *
 * @v image		ELF file
 * @v ehdr		ELF executable header
 * @v phdr		ELF program header
 * @v process		Segment processor
 * @ret entry		Entry point, if found
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
static int elf_segment ( struct image *image, const Elf_Ehdr *ehdr,
			 const Elf_Phdr *phdr,
			 int ( * process ) ( struct image *image,
					     const Elf_Phdr *phdr,
					     physaddr_t dest ),
			 physaddr_t *entry, physaddr_t *max ) {
	physaddr_t dest;
	physaddr_t end;
	unsigned long e_offset;
	int rc;

	/* Do nothing for non-PT_LOAD segments */
	if ( phdr->p_type != PT_LOAD )
		return 0;

	/* Check segment lies within image */
	if ( ( phdr->p_offset + phdr->p_filesz ) > image->len ) {
		DBGC ( image, "ELF %s segment outside image\n", image->name );
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
		DBGC ( image, "ELF %s segment loads to physical address 0\n",
		       image->name );
		return -ENOEXEC;
	}
	end = ( dest + phdr->p_memsz );

	/* Update maximum used address, if applicable */
	if ( end > *max )
		*max = end;

	/* Process segment */
	if ( ( rc = process ( image, phdr, dest ) ) != 0 )
		return rc;

	/* Set execution address, if it lies within this segment */
	if ( ( e_offset = ( ehdr->e_entry - dest ) ) < phdr->p_filesz ) {
		*entry = ehdr->e_entry;
		DBGC ( image, "ELF %s found physical entry point at %lx\n",
		       image->name, *entry );
	} else if ( ( e_offset = ( ehdr->e_entry - phdr->p_vaddr ) )
		    < phdr->p_filesz ) {
		if ( ! *entry ) {
			*entry = ( dest + e_offset );
			DBGC ( image, "ELF %s found virtual entry point at %lx"
			       " (virt %lx)\n", image->name, *entry,
			       ( ( unsigned long ) ehdr->e_entry ) );
		}
	}

	return 0;
}

/**
 * Process ELF segments
 *
 * @v image		ELF file
 * @v ehdr		ELF executable header
 * @v process		Segment processor
 * @ret entry		Entry point, if found
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
int elf_segments ( struct image *image, const Elf_Ehdr *ehdr,
		   int ( * process ) ( struct image *image,
				       const Elf_Phdr *phdr,
				       physaddr_t dest ),
		   physaddr_t *entry, physaddr_t *max ) {
	const Elf_Phdr *phdr;
	Elf_Off phoff;
	unsigned int phnum;
	int rc;

	/* Initialise maximum used address */
	*max = 0;

	/* Invalidate entry point */
	*entry = 0;

	/* Read and process ELF program headers */
	for ( phoff = ehdr->e_phoff , phnum = ehdr->e_phnum ; phnum ;
	      phoff += ehdr->e_phentsize, phnum-- ) {
		if ( ( image->len < phoff ) ||
		     ( ( image->len - phoff ) < sizeof ( *phdr ) ) ) {
			DBGC ( image, "ELF %s program header %d outside "
			       "image\n", image->name, phnum );
			return -ENOEXEC;
		}
		phdr = ( image->data + phoff );
		if ( ( rc = elf_segment ( image, ehdr, phdr, process,
					  entry, max ) ) != 0 )
			return rc;
	}

	/* Check for a valid execution address */
	if ( ! *entry ) {
		DBGC ( image, "ELF %s entry point %lx outside image\n",
		       image->name, ( ( unsigned long ) ehdr->e_entry ) );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Load ELF image into memory
 *
 * @v image		ELF file
 * @ret entry		Entry point
 * @ret max		Maximum used address
 * @ret rc		Return status code
 */
int elf_load ( struct image *image, physaddr_t *entry, physaddr_t *max ) {
	static const uint8_t e_ident[] = {
		[EI_MAG0]	= ELFMAG0,
		[EI_MAG1]	= ELFMAG1,
		[EI_MAG2]	= ELFMAG2,
		[EI_MAG3]	= ELFMAG3,
		[EI_CLASS]	= ELFCLASS,
	};
	const Elf_Ehdr *ehdr;
	int rc;

	/* Read ELF header */
	if ( image->len < sizeof ( *ehdr ) ) {
		DBGC ( image, "ELF %s too short for ELF header\n",
		       image->name );
		return -ENOEXEC;
	}
	ehdr = image->data;
	if ( memcmp ( ehdr->e_ident, e_ident, sizeof ( e_ident ) ) != 0 ) {
		DBGC ( image, "ELF %s has invalid signature\n", image->name );
		return -ENOEXEC;
	}

	/* Load ELF segments into memory */
	if ( ( rc = elf_segments ( image, ehdr, elf_load_segment,
				   entry, max ) ) != 0 )
		return rc;

	return 0;
}
