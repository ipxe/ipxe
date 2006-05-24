/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <errno.h>
#include <realmode.h>
#include <bios.h>
#include <memmap.h>

/**
 * @file
 *
 * Memory mapping
 *
 */

/** Magic value for INT 15,e820 calls */
#define SMAP ( 0x534d4150 )

/** An INT 15,e820 memory map entry */
struct e820_entry {
	/** Start of region */
	uint64_t start;
	/** Length of region */
	uint64_t len;
	/** Type of region */
	uint32_t type;
} __attribute__ (( packed ));

#define E820_TYPE_RAM		1 /**< Normal memory */
#define E820_TYPE_RESERVED	2 /**< Reserved and unavailable */
#define E820_TYPE_ACPI		3 /**< ACPI reclaim memory */
#define E820_TYPE_NVS		4 /**< ACPI NVS memory */

/** Buffer for INT 15,e820 calls */
static struct e820_entry __data16 ( e820buf );
#define e820buf __use_data16 ( e820buf )

/**
 * Get size of base memory from BIOS free base memory counter
 *
 * @ret basemem		Base memory size, in kB
 */
static unsigned int basememsize ( void ) {
	uint16_t basemem;

	get_real ( basemem, BDA_SEG, 0x0013 );
	DBG ( "Base memory size %dkB\n", basemem );
	return basemem;
}

/**
 * Get size of extended memory via INT 15,e801
 *
 * @ret extmem		Extended memory size, in kB, or 0
 */
static unsigned int extmemsize_e801 ( void ) {
	uint16_t extmem_1m_to_16m_k, extmem_16m_plus_64k;
	uint16_t confmem_1m_to_16m_k, confmem_16m_plus_64k;
	unsigned int flags;
	unsigned int extmem;

	__asm__ __volatile__ ( REAL_CODE ( "stc\n\t"
					   "int $0x15\n\t"
					   "pushfw\n\t"
					   "popw %w0\n\t" )
			       : "=r" ( flags ),
				 "=a" ( extmem_1m_to_16m_k ),
				 "=b" ( extmem_16m_plus_64k ),
				 "=c" ( confmem_1m_to_16m_k ),
				 "=d" ( confmem_16m_plus_64k )
			       : "a" ( 0xe801 ) );

	if ( flags & CF )
		return 0;

	if ( ! ( extmem_1m_to_16m_k | extmem_16m_plus_64k ) ) {
		extmem_1m_to_16m_k = confmem_1m_to_16m_k;
		extmem_16m_plus_64k = confmem_16m_plus_64k;
	}

	extmem = ( extmem_1m_to_16m_k + ( extmem_16m_plus_64k * 64 ) );
	DBG ( "Extended memory size %d+64*%d=%d kB\n",
	      extmem_1m_to_16m_k, extmem_16m_plus_64k, extmem );
	return extmem;
}

/**
 * Get size of extended memory via INT 15,88
 *
 * @ret extmem		Extended memory size, in kB
 */
static unsigned int extmemsize_88 ( void ) {
	uint16_t extmem;

	/* Ignore CF; it is not reliable for this call */
	__asm__ __volatile__ ( REAL_CODE ( "int $0x15" )
			       : "=a" ( extmem ) : "a" ( 0x8800 ) );

	DBG ( "Extended memory size %d kB\n", extmem );
	return extmem;
}

/**
 * Get size of extended memory
 *
 * @ret extmem		Extended memory size, in kB
 */
static unsigned int extmemsize ( void ) {
	unsigned int extmem;

	/* Try INT 15,e801 first, then fall back to INT 15,88 */
	extmem = extmemsize_e801();
	if ( ! extmem )
		extmem = extmemsize_88();
	return extmem;
}

/**
 * Get e820 memory map
 *
 * @v memmap		Memory map to fill in
 * @v entries		Maximum number of entries in memory map
 * @ret rc		Return status code
 */
static int meme820 ( struct memory_region *memmap, unsigned int entries ) {
	unsigned int index = 0;
	uint32_t next = 0;
	uint32_t smap;
	unsigned int flags;
	unsigned int discard_c, discard_d, discard_D;

	do {
		__asm__ __volatile__ ( REAL_CODE ( "stc\n\t"
						   "int $0x15\n\t"
						   "pushfw\n\t"
						   "popw %w0\n\t" )
				       : "=r" ( flags ), "=a" ( smap ),
					 "=b" ( next ), "=D" ( discard_D ),
					 "=c" ( discard_c ), "=d" ( discard_d )
				       : "a" ( 0xe820 ), "b" ( next ),
					 "D" ( &__from_data16 ( e820buf ) ),
					 "c" ( sizeof ( e820buf ) ),
					 "d" ( SMAP )
				       : "memory" );

		if ( smap != SMAP )
			return -ENOTSUP;

		if ( flags & CF )
			break;

		DBG ( "E820 region [%llx,%llx) type %d\n", e820buf.start,
		      ( e820buf.start + e820buf.len ), ( int ) e820buf.type );
		if ( e820buf.type != E820_TYPE_RAM )
			continue;

		memmap[index].start = e820buf.start;
		memmap[index].end = e820buf.start + e820buf.len;
		index++;
	} while ( ( index < entries ) && ( next != 0 ) );
	return 0;
}

/**
 * Get memory map
 *
 * @v memmap		Memory map to fill in
 * @v entries		Maximum number of entries in memory map (minimum 2)
 */
void get_memmap ( struct memory_region *memmap, unsigned int entries ) {
	unsigned int basemem, extmem;
	int rc;

	/* Clear memory map */
	memset ( memmap, 0, ( entries * sizeof ( *memmap ) ) );

	/* Get base and extended memory sizes */
	basemem = basememsize();
	extmem = extmemsize();
	
	/* Try INT 15,e820 first */
	if ( ( rc = meme820 ( memmap, entries ) ) == 0 )
		return;

	/* Fall back to constructing a map from basemem and extmem sizes */
	memmap[0].end = ( basemem * 1024 );
	memmap[1].start = 0x100000;
	memmap[1].end = 0x100000 + ( extmem * 1024 );
}
