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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <errno.h>
#include <elf.h>
#include <stdio.h>
#include <ipxe/image.h>
#include <ipxe/elf.h>
#include <ipxe/features.h>
#include <ipxe/init.h>
#include <ipxe/io.h>

typedef uint64_t p4_entry_t;
typedef uint64_t p3_entry_t;
typedef uint64_t p2_entry_t;
extern p4_entry_t PT4[];
extern p3_entry_t PT3[];
extern p2_entry_t PT2[];
extern char * BLAH;

uint32_t entry_hi;
uint32_t entry_lo;

extern void amd64_tramp();
#define	VTOP(va)	virt_to_phys(va)
typedef char * caddr_t;
void __exec(caddr_t, ...);

#define ALIGN_UP(value, align)  (((value) & (align-1)) ?                \
                                (((value) + (align-1)) & ~(align-1)) : \
                                (value))
#define ALIGN_PAGE(a)           ALIGN_UP (a, 4096)

#define PG_V	0x001
#define PG_RW	0x002
#define PG_U	0x004
#define PG_PS	0x080

/**
 * @file
 *
 * ELF bootable image
 *
 */

FEATURE ( FEATURE_IMAGE, "ELF64", DHCP_EB_FEATURE_ELF, 1 );

/**
 * Prepare segment for loading
 *
 * @v segment		Segment start
 * @v filesz		Size of the "allocated bytes" portion of the segment
 * @v memsz		Size of the segment
 * @ret rc		Return status code
 */
static int allocate_segment ( physaddr_t * segment, size_t sz ) {
	struct memory_map memmap;
	unsigned int i;

	get_memmap ( &memmap );

	/* Look for a suitable memory region */
	for ( i = 0 ; i < memmap.count ; i++ ) {
		if ( (memmap.regions[i].end - memmap.regions[i].start) > sz) {
			*segment = memmap.regions[i].start;
			memset_user ( *segment, 0, 0, sz);
			return 0;
		}
	}

	return -1;
}

/**
 * Execute ELF image
 *
 * @v image		ELF image
 * @ret rc		Return status code
 */
static int elfboot64_exec ( struct image *image ) {
	int i;

	size_t size = 0;
	physaddr_t buffer = 0;

	unsigned long kern_end, modulep = 0;
	struct image *module_image;
	Elf64_Ehdr ehdr;

	// Get total size
	for_each_image ( module_image ) {
		size += ALIGN_PAGE(image->len);
	}
	//XXX: size += ALIGN_PAGE(params)
	//XXX: size += ALIGN_PAGE(env)

	if (allocate_segment(&buffer, size) != 0) {
		DBG ( "Couldn't allocate enough memory to fit kernel, needed %d bytes", size);
		return -1;
	}


	modulep = buffer;

	// Copy kernel
	memcpy_user(phys_to_user(buffer), 0, image->data, 0, image->len);
	buffer += ALIGN_PAGE(image->len);
	copy_from_user ( &ehdr, image->data, 0, sizeof ( ehdr ) );

	// Copy image
	for_each_image ( module_image ) {
		if (image == module_image) continue;
		memcpy_user(phys_to_user(buffer), 0, module_image->data, 0, module_image->len);
		buffer += ALIGN_PAGE(module_image->len);
	}

	kern_end = buffer;

	entry_lo = ehdr.e_entry & 0xffffffff;
	entry_hi = (ehdr.e_entry >> 32) & 0xffffffff;

	for (i = 0; i < 512; i++) {
		/* Each slot of the level 4 pages points to the same level 3 page */
		PT4[i] = (p4_entry_t)VTOP((void *)&PT3[0]);
		PT4[i] |= PG_V | PG_RW | PG_U;

		/* Each slot of the level 3 pages points to the same level 2 page */
		PT3[i] = (p3_entry_t)VTOP((void *)&PT2[0]);
		PT3[i] |= PG_V | PG_RW | PG_U;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		PT2[i] = i * (2 * 1024 * 1024);
		PT2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}

	/*
	  stack[0] = VTOP(amd64_tramp)
	  stack[1] = modulep
	  stack[2] = kern_end
	*/
	__exec((void *)VTOP(amd64_tramp), modulep, kern_end);

	DBG("exec returned, this is wrong, very wrong\n");

	return -1;  /* -EIMPOSSIBLE, anyone? */
}

/**
 * Probe ELF image
 *
 * @v image		ELF file
 * @ret rc		Return status code
 */
static int elfboot64_probe ( struct image *image ) {
	Elf64_Ehdr ehdr;
	static const uint8_t e_ident[] = {
		[EI_MAG0]	= ELFMAG0,
		[EI_MAG1]	= ELFMAG1,
		[EI_MAG2]	= ELFMAG2,
		[EI_MAG3]	= ELFMAG3,
		[EI_CLASS]	= ELFCLASS64,
		[EI_DATA]	= ELFDATA2LSB,
		[EI_VERSION]	= EV_CURRENT,
	};

	/* Read ELF header */
	copy_from_user ( &ehdr, image->data, 0, sizeof ( ehdr ) );
	if ( memcmp ( ehdr.e_ident, e_ident, sizeof ( e_ident ) ) != 0 ) {
		DBG ( "Invalid ELF identifier\n" );
		return -1;
	}

	return 0;
}

/** ELF image type */
struct image_type freebsd_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "ELF64",
	.probe = elfboot64_probe,
	.exec = elfboot64_exec,
};
