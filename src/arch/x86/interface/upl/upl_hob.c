/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
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

/** @file
 *
 * Universal Payload Hand-Off Block (HOB) list
 *
 * A bootloader may pass a HOB list in place of a device tree (e.g.
 * Slim Bootloader).  We construct the equivalent device tree, using
 * the node names and properties produced by EDK2's BuildFdtLib.
 *
 */

#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/uaccess.h>
#include <ipxe/fdt.h>
#include <ipxe/upl.h>

/** Maximum length of constructed device tree structure block */
#define UPL_HOB_FDT_STRUCT_LEN 16384

/** Maximum length of constructed device tree strings block */
#define UPL_HOB_FDT_STRINGS_LEN 256

/** Length of an ACPI RSDP (version 2) */
#define UPL_HOB_RSDP_LEN 36

/** Length of region containing an SMBIOS entry point */
#define UPL_HOB_SMBIOS_LEN 0x20

/** HOB list provided by the bootloader */
static const struct upl_hob_header *upl_hob_list;

/** Constructed device tree */
static struct {
	/** Header */
	struct fdt_header hdr;
	/** Memory reservation block (empty) */
	struct fdt_reservation rsv;
	/** Structure block */
	uint32_t structure[ UPL_HOB_FDT_STRUCT_LEN / sizeof ( uint32_t ) ];
	/** Strings block */
	char strings[UPL_HOB_FDT_STRINGS_LEN];
} __attribute__ (( aligned ( 8 ) )) upl_hob_tree;

/** Used length of constructed structure block */
static size_t upl_hob_struct_len;

/** Used length of constructed strings block */
static size_t upl_hob_strings_len;

/** Constructed device tree has overflowed */
static int upl_hob_overflow;

/** Memory allocation names (as used by EDK2's BuildFdtLib) */
static const char *upl_hob_names[] = {
	"Reserved", "LoaderCode", "LoaderData", "boot-code", "boot-data",
	"runtime-code", "runtime-data", "ConventionalMemory",
	"UnusableMemory", "acpi", "acpi-nvs", "mmio",
	"MemoryMappedIOPortSpace", "PalCode", "PersistentMemory",
};

/** Number of memory allocation names */
#define UPL_HOB_NAMES \
	( sizeof ( upl_hob_names ) / sizeof ( upl_hob_names[0] ) )

/**
 * Check HOB validity
 *
 * @v hob		HOB
 * @ret hob		HOB, or NULL at end of list or if malformed
 */
static const struct upl_hob_header *
upl_hob_check ( const struct upl_hob_header *hob ) {

	if ( hob->type == UPL_HOB_END )
		return NULL;
	if ( hob->len < sizeof ( *hob ) ) {
		DBGC ( &upl_hob_list, "UPL malformed HOB at %p\n", hob );
		return NULL;
	}
	return hob;
}

/**
 * Get first HOB
 *
 * @ret hob		First HOB, or NULL
 */
static const struct upl_hob_header * upl_hob_first ( void ) {

	if ( ! upl_hob_list )
		return NULL;
	return upl_hob_check ( upl_hob_list );
}

/**
 * Get next HOB
 *
 * @v hob		Current HOB
 * @ret next		Next HOB, or NULL
 */
static const struct upl_hob_header *
upl_hob_next ( const struct upl_hob_header *hob ) {

	return upl_hob_check ( ( ( const void * ) hob ) + hob->len );
}

/**
 * Iterate over HOB list
 *
 * @v hob		HOB pointer
 */
#define for_each_upl_hob( hob ) \
	for ( (hob) = upl_hob_first() ; (hob) ; (hob) = upl_hob_next ( hob ) )

/**
 * Find GUID extension HOB data
 *
 * @v guid		GUID
 * @v len		Length of HOB data to fill in
 * @ret data		HOB data, or NULL if not found
 */
static const void * upl_hob_data ( const struct upl_guid *guid,
				  size_t *len ) {
	const struct upl_hob_header *hob;
	const struct upl_guid_hob *guid_hob;

	for_each_upl_hob ( hob ) {
		if ( hob->type != UPL_HOB_GUID_EXTENSION )
			continue;
		if ( hob->len < sizeof ( *guid_hob ) )
			continue;
		guid_hob = container_of ( hob, struct upl_guid_hob, hdr );
		if ( memcmp ( &guid_hob->name, guid, sizeof ( *guid ) ) != 0 )
			continue;
		*len = ( hob->len - sizeof ( *guid_hob ) );
		return ( guid_hob + 1 );
	}

	DBGC ( &upl_hob_list, "UPL found no HOB %08x-%04x-%04x\n",
	       guid->a, guid->b, guid->c );
	return NULL;
}

/**
 * Find GUID extension HOB
 *
 * @v guid		GUID
 * @v len		Minimum required length of HOB data
 * @ret data		HOB data, or NULL if not found
 */
static const void * upl_hob ( const struct upl_guid *guid, size_t len ) {
	const void *data;
	size_t actual;

	data = upl_hob_data ( guid, &actual );
	if ( data && ( actual < len ) ) {
		DBGC ( &upl_hob_list, "UPL HOB %08x-%04x-%04x too short\n",
		       guid->a, guid->b, guid->c );
		return NULL;
	}
	return data;
}

/**
 * Append data to constructed structure block
 *
 * @v data		Data
 * @v len		Length of data
 */
static void upl_hob_fdt_append ( const void *data, size_t len ) {
	void *dest = ( ( ( void * ) upl_hob_tree.structure ) +
		       upl_hob_struct_len );
	size_t padded = ( ( len + sizeof ( uint32_t ) - 1 ) &
			  ~( sizeof ( uint32_t ) - 1 ) );

	if ( ! len )
		return;
	if ( ( upl_hob_struct_len + padded ) >
	     sizeof ( upl_hob_tree.structure ) ) {
		upl_hob_overflow = 1;
		return;
	}
	memcpy ( dest, data, len );
	memset ( ( dest + len ), 0, ( padded - len ) );
	upl_hob_struct_len += padded;
}

/**
 * Append token to constructed structure block
 *
 * @v token		Token
 */
static void upl_hob_fdt_token ( uint32_t token ) {
	uint32_t value = cpu_to_be32 ( token );

	upl_hob_fdt_append ( &value, sizeof ( value ) );
}

/**
 * Begin node
 *
 * @v name		Node name
 */
static void upl_hob_fdt_begin ( const char *name ) {

	upl_hob_fdt_token ( FDT_BEGIN_NODE );
	upl_hob_fdt_append ( name, ( strlen ( name ) + 1 /* NUL */ ) );
}

/**
 * Begin node with unit address
 *
 * @v name		Node name
 * @v addr		Unit address
 */
static void upl_hob_fdt_begin_at ( const char *name, uint64_t addr ) {
	char buf[32];

	snprintf ( buf, sizeof ( buf ), "%s@%llx", name,
		   ( ( unsigned long long ) addr ) );
	upl_hob_fdt_begin ( buf );
}

/**
 * End node
 *
 */
static void upl_hob_fdt_end ( void ) {

	upl_hob_fdt_token ( FDT_END_NODE );
}

/**
 * Add property
 *
 * @v name		Property name
 * @v data		Property value
 * @v len		Length of property value
 */
static void upl_hob_fdt_prop ( const char *name, const void *data,
			       size_t len ) {
	struct fdt_prop prop;
	size_t name_len = ( strlen ( name ) + 1 /* NUL */ );
	size_t offset;

	/* Locate or add property name within strings block */
	for ( offset = 0 ; offset < upl_hob_strings_len ;
	      offset += ( strlen ( &upl_hob_tree.strings[offset] ) + 1 ) ) {
		if ( strcmp ( &upl_hob_tree.strings[offset], name ) == 0 )
			break;
	}
	if ( offset == upl_hob_strings_len ) {
		if ( ( offset + name_len ) > sizeof ( upl_hob_tree.strings ) ){
			upl_hob_overflow = 1;
			return;
		}
		memcpy ( &upl_hob_tree.strings[offset], name, name_len );
		upl_hob_strings_len += name_len;
	}

	/* Append property */
	upl_hob_fdt_token ( FDT_PROP );
	prop.len = cpu_to_be32 ( len );
	prop.name_off = cpu_to_be32 ( offset );
	upl_hob_fdt_append ( &prop, sizeof ( prop ) );
	upl_hob_fdt_append ( data, len );
}

/**
 * Add string property
 *
 * @v name		Property name
 * @v value		String value
 */
static void upl_hob_fdt_string ( const char *name, const char *value ) {

	upl_hob_fdt_prop ( name, value, ( strlen ( value ) + 1 /* NUL */ ) );
}

/**
 * Add property comprising 32-bit cells
 *
 * @v name		Property name
 * @v cells		Cell values
 * @v count		Number of cells
 */
static void upl_hob_fdt_cells ( const char *name, const uint32_t *cells,
				unsigned int count ) {
	uint32_t data[count];
	unsigned int i;

	for ( i = 0 ; i < count ; i++ )
		data[i] = cpu_to_be32 ( cells[i] );
	upl_hob_fdt_prop ( name, data, sizeof ( data ) );
}

/**
 * Add 32-bit integer property
 *
 * @v name		Property name
 * @v value		Value
 */
static void upl_hob_fdt_u32 ( const char *name, uint32_t value ) {

	upl_hob_fdt_cells ( name, &value, 1 );
}

/**
 * Add region property (with two address cells and two size cells)
 *
 * @v start		Starting address
 * @v size		Size
 */
static void upl_hob_fdt_reg ( uint64_t start, uint64_t size ) {
	uint32_t cells[4] = { ( start >> 32 ), start, ( size >> 32 ), size };

	upl_hob_fdt_cells ( "reg", cells, 4 );
}

/**
 * Add memory node
 *
 * @v name		Node name
 * @v start		Starting address
 * @v size		Size
 * @v compatible	Compatible string, or NULL
 */
static void upl_hob_fdt_memory ( const char *name, uint64_t start,
				 uint64_t size, const char *compatible ) {

	DBGC ( &upl_hob_list, "UPL HOB %s [%#08llx,%#08llx) %s\n", name,
	       ( ( unsigned long long ) start ),
	       ( ( unsigned long long ) ( start + size ) ),
	       ( compatible ? compatible : "" ) );
	upl_hob_fdt_begin_at ( name, start );
	upl_hob_fdt_reg ( start, size );
	if ( compatible )
		upl_hob_fdt_string ( "compatible", compatible );
}

/**
 * Describe system memory
 *
 */
static void upl_hob_fdt_system_memory ( void ) {
	const struct upl_hob_header *hob;
	const struct upl_resource_hob *resource;

	for_each_upl_hob ( hob ) {
		if ( hob->type != UPL_HOB_RESOURCE_DESCRIPTOR )
			continue;
		if ( hob->len < sizeof ( *resource ) )
			continue;
		resource = container_of ( hob, struct upl_resource_hob, hdr );
		if ( resource->type != UPL_RESOURCE_SYSTEM_MEMORY )
			continue;
		upl_hob_fdt_memory ( "memory", resource->start, resource->len,
				     NULL );
		upl_hob_fdt_string ( "device_type", "memory" );
		upl_hob_fdt_end();
	}
}

/**
 * Describe reserved memory
 *
 */
static void upl_hob_fdt_reserved_memory ( void ) {
	static const struct upl_guid acpi_guid = UPL_ACPI_GUID;
	static const struct upl_guid smbios_guid = UPL_SMBIOS_GUID;
	static const struct upl_guid smbios3_guid = UPL_SMBIOS3_GUID;
	const struct upl_hob_header *hob;
	const struct upl_handoff_hob *handoff;
	const struct upl_resource_hob *resource;
	const struct upl_memalloc_hob *memalloc;
	const struct upl_acpi *acpi;
	const struct upl_smbios *smbios;
	const char *compatible;
	uint64_t rsdp;
	int have_rsdp = 0;

	/* Identify ACPI RSDP, if present */
	acpi = upl_hob ( &acpi_guid, sizeof ( *acpi ) );
	rsdp = ( ( acpi && ( acpi->hdr.len >= sizeof ( *acpi ) ) ) ?
		 acpi->rsdp : 0 );

	upl_hob_fdt_begin ( "reserved-memory" );
	upl_hob_fdt_u32 ( "#address-cells", 2 );
	upl_hob_fdt_u32 ( "#size-cells", 2 );
	upl_hob_fdt_prop ( "ranges", NULL, 0 );
	for_each_upl_hob ( hob ) {
		switch ( hob->type ) {

		case UPL_HOB_HANDOFF:
			if ( hob->len < sizeof ( *handoff ) )
				break;
			handoff = container_of ( hob, struct upl_handoff_hob,
						 hdr );
			if ( handoff->memory_top <= handoff->memory_bottom )
				break;
			upl_hob_fdt_memory ( "memory", handoff->memory_bottom,
					     ( handoff->memory_top -
					       handoff->memory_bottom ),
					     NULL );
			upl_hob_fdt_end();
			break;

		case UPL_HOB_RESOURCE_DESCRIPTOR:
			if ( hob->len < sizeof ( *resource ) )
				break;
			resource = container_of ( hob, struct upl_resource_hob,
						  hdr );
			if ( resource->type != UPL_RESOURCE_MEMORY_RESERVED )
				break;
			upl_hob_fdt_memory ( "memory", resource->start,
					     resource->len, NULL );
			upl_hob_fdt_end();
			break;

		case UPL_HOB_MEMORY_ALLOCATION:
			if ( hob->len < sizeof ( *memalloc ) )
				break;
			memalloc = container_of ( hob, struct upl_memalloc_hob,
						  hdr );
			if ( memalloc->type == UPL_MEMORY_CONVENTIONAL )
				break;
			compatible = upl_hob_names[0];
			if ( memalloc->type < UPL_HOB_NAMES )
				compatible = upl_hob_names[memalloc->type];
			if ( ( memalloc->type == UPL_MEMORY_ACPI_RECLAIM ) &&
			     ( memalloc->base == rsdp ) ) {
				have_rsdp = 1;
			}
			upl_hob_fdt_memory ( ( ( strcmp ( compatible,
							  "mmio" ) == 0 ) ?
					       "mmio" : "memory" ),
					     memalloc->base, memalloc->len,
					     compatible );
			upl_hob_fdt_end();
			break;

		default:
			break;
		}
	}

	/* Describe ACPI RSDP, if not already described */
	if ( rsdp && ! have_rsdp ) {
		upl_hob_fdt_memory ( "memory", rsdp, UPL_HOB_RSDP_LEN, "acpi" );
		upl_hob_fdt_end();
	}

	/* Describe SMBIOS entry point, preferring SMBIOS 2.x */
	smbios = upl_hob ( &smbios_guid, sizeof ( *smbios ) );
	if ( ! ( smbios && ( smbios->hdr.len >= sizeof ( *smbios ) ) &&
		 smbios->entry ) ) {
		smbios = upl_hob ( &smbios3_guid, sizeof ( *smbios ) );
	}
	if ( smbios && ( smbios->hdr.len >= sizeof ( *smbios ) ) &&
	     smbios->entry ) {
		upl_hob_fdt_memory ( "memory", smbios->entry,
				     UPL_HOB_SMBIOS_LEN, "smbios" );
		upl_hob_fdt_end();
	}
	upl_hob_fdt_end();
}

/**
 * Describe serial port
 *
 */
static void upl_hob_fdt_serial ( void ) {
	static const struct upl_guid guid = UPL_SERIAL_GUID;
	const struct upl_serial *serial;
	unsigned int shift;
	uint32_t io[3];

	/* Locate serial port information HOB */
	serial = upl_hob ( &guid, sizeof ( *serial ) );
	if ( ! ( serial && ( serial->hdr.len >= sizeof ( *serial ) ) ) )
		return;
	for ( shift = 0 ; ( shift < 8 ) &&
		      ( ( 1U << shift ) < serial->stride ) ; shift++ ) {}
	DBGC ( &upl_hob_list, "UPL HOB serial %s %#08llx stride %d baud %d\n",
	       ( serial->mmio ? "MMIO" : "I/O" ),
	       ( ( unsigned long long ) serial->base ), serial->stride,
	       serial->baud );

	/* Legacy I/O ports are described within an ISA bus node */
	if ( serial->mmio ) {
		upl_hob_fdt_begin_at ( "serial", serial->base );
		upl_hob_fdt_reg ( serial->base, ( 8 << shift ) );
	} else {
		upl_hob_fdt_begin ( "isa" );
		upl_hob_fdt_string ( "compatible", "isa" );
		upl_hob_fdt_u32 ( "#address-cells", 2 );
		upl_hob_fdt_u32 ( "#size-cells", 1 );
		upl_hob_fdt_begin_at ( "serial", serial->base );
		io[0] = 1;
		io[1] = serial->base;
		io[2] = ( 8 << shift );
		upl_hob_fdt_cells ( "reg", io, 3 );
	}
	if ( serial->baud )
		upl_hob_fdt_u32 ( "current-speed", serial->baud );
	if ( shift )
		upl_hob_fdt_u32 ( "reg-shift", shift );
	upl_hob_fdt_end();
	if ( ! serial->mmio )
		upl_hob_fdt_end();
}

/**
 * Describe frame buffer
 *
 */
static void upl_hob_fdt_framebuffer ( void ) {
	static const struct upl_guid guid = UPL_GRAPHICS_GUID;
	const struct upl_graphics *gfx;
	const char *format;
	unsigned int len;

	/* Locate graphics information HOB */
	gfx = upl_hob ( &guid, sizeof ( *gfx ) );
	if ( ! ( gfx && gfx->base ) )
		return;

	/* Identify pixel format (as a little-endian pixel value) */
	if ( ( gfx->format == UPL_PIXEL_BGRX ) ||
	     ( ( gfx->format == UPL_PIXEL_BITMASK ) &&
	       ( gfx->red_mask == 0x00ff0000UL ) &&
	       ( gfx->green_mask == 0x0000ff00UL ) &&
	       ( gfx->blue_mask == 0x000000ffUL ) ) ) {
		format = "a8r8g8b8";
		len = 4;
	} else if ( ( gfx->format == UPL_PIXEL_RGBX ) ||
		    ( ( gfx->format == UPL_PIXEL_BITMASK ) &&
		      ( gfx->red_mask == 0x000000ffUL ) &&
		      ( gfx->green_mask == 0x0000ff00UL ) &&
		      ( gfx->blue_mask == 0x00ff0000UL ) ) ) {
		format = "a8b8g8r8";
		len = 4;
	} else if ( ( gfx->format == UPL_PIXEL_BITMASK ) &&
		    ( gfx->red_mask == 0xf800 ) &&
		    ( gfx->green_mask == 0x07e0 ) &&
		    ( gfx->blue_mask == 0x001f ) ) {
		format = "r5g6b5";
		len = 2;
	} else {
		DBGC ( &upl_hob_list, "UPL HOB unsupported pixel format %d\n",
		       gfx->format );
		return;
	}
	DBGC ( &upl_hob_list, "UPL HOB framebuffer %dx%d %s at %#08llx\n",
	       gfx->width, gfx->height, format,
	       ( ( unsigned long long ) gfx->base ) );

	/* Describe frame buffer */
	upl_hob_fdt_begin_at ( "framebuffer", gfx->base );
	upl_hob_fdt_string ( "compatible", "simple-framebuffer" );
	upl_hob_fdt_reg ( gfx->base, gfx->size );
	upl_hob_fdt_u32 ( "width", gfx->width );
	upl_hob_fdt_u32 ( "height", gfx->height );
	upl_hob_fdt_u32 ( "stride", ( gfx->stride * len ) );
	upl_hob_fdt_string ( "format", format );
	upl_hob_fdt_end();
}

/**
 * Construct device tree from HOB list
 *
 * @v hob_list		HOB list provided by the bootloader
 * @ret hdr		Device tree, or NULL on error
 */
struct fdt_header * upl_hob_fdt ( const void *hob_list ) {
	const struct upl_hob_header *hob = hob_list;
	struct fdt_header *hdr = &upl_hob_tree.hdr;
	size_t len;

	/* Record HOB list, which must start with a PHIT HOB */
	if ( hob->type != UPL_HOB_HANDOFF ) {
		DBGC ( &upl_hob_list, "UPL unrecognised hand-off at %p\n",
		       hob_list );
		return NULL;
	}
	upl_hob_list = hob;

	/* Construct structure block */
	upl_hob_fdt_begin ( "" );
	upl_hob_fdt_u32 ( "#address-cells", 2 );
	upl_hob_fdt_u32 ( "#size-cells", 2 );
	upl_hob_fdt_system_memory();
	upl_hob_fdt_reserved_memory();
	upl_hob_fdt_serial();
	upl_hob_fdt_framebuffer();
	upl_hob_fdt_end();
	upl_hob_fdt_token ( FDT_END );
	if ( upl_hob_overflow ) {
		DBGC ( &upl_hob_list, "UPL HOB device tree overflow\n" );
		return NULL;
	}

	/* Move strings block to follow structure block */
	memmove ( ( ( ( void * ) upl_hob_tree.structure ) +
		    upl_hob_struct_len ), upl_hob_tree.strings,
		  upl_hob_strings_len );

	/* Construct header */
	len = ( offsetof ( typeof ( upl_hob_tree ), structure ) +
		upl_hob_struct_len + upl_hob_strings_len );
	hdr->magic = cpu_to_be32 ( FDT_MAGIC );
	hdr->totalsize = cpu_to_be32 ( len );
	hdr->off_dt_struct =
		cpu_to_be32 ( offsetof ( typeof ( upl_hob_tree ), structure ) );
	hdr->off_dt_strings =
		cpu_to_be32 ( offsetof ( typeof ( upl_hob_tree ), structure ) +
			      upl_hob_struct_len );
	hdr->off_mem_rsvmap =
		cpu_to_be32 ( offsetof ( typeof ( upl_hob_tree ), rsv ) );
	hdr->version = cpu_to_be32 ( FDT_VERSION + 1 );
	hdr->last_comp_version = cpu_to_be32 ( FDT_VERSION );
	hdr->size_dt_strings = cpu_to_be32 ( upl_hob_strings_len );
	hdr->size_dt_struct = cpu_to_be32 ( upl_hob_struct_len );

	return hdr;
}
