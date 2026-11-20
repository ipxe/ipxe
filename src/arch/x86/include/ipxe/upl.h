#ifndef _IPXE_UPL_H
#define _IPXE_UPL_H

/** @file
 *
 * Universal Payload (UPL) interface
 *
 * The bootloader passes a Flattened Device Tree describing the memory
 * map, serial port, ACPI tables, SMBIOS tables, and graphics frame
 * buffer.  A bootloader may instead pass a list of Hand-Off Blocks
 * (HOBs) as defined by the Platform Initialization (PI) specification
 * and extended by the Universal Payload specification (e.g. Slim
 * Bootloader), from which we construct an equivalent device tree.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

struct fdt_header;

/** Memory region types */
enum upl_memory_type {
	/** Usable RAM */
	UPL_MEM_RAM,
	/** Reserved */
	UPL_MEM_RESERVED,
	/** Used only by the bootloader (and usable by an operating system) */
	UPL_MEM_BOOT,
	/** ACPI tables */
	UPL_MEM_ACPI,
	/** ACPI non-volatile storage */
	UPL_MEM_NVS,
};

/** A linear frame buffer */
struct upl_framebuffer {
	/** Physical base address */
	uint64_t base;
	/** Length */
	uint64_t size;
	/** Width (in pixels) */
	unsigned int width;
	/** Height (in pixels) */
	unsigned int height;
	/** Line length (in bytes) */
	unsigned int stride;
	/** Red mask */
	uint32_t red_mask;
	/** Green mask */
	uint32_t green_mask;
	/** Blue mask */
	uint32_t blue_mask;
	/** Reserved mask */
	uint32_t reserved_mask;
};

/** A GUID as used within HOBs */
struct upl_guid {
	uint32_t a;
	uint16_t b;
	uint16_t c;
	uint8_t d[8];
} __attribute__ (( packed ));

/** Construct a HOB GUID */
#define UPL_GUID( a, b, c, d0, d1, d2, d3, d4, d5, d6, d7 ) \
	{ a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }

/** A generic HOB header (EFI_HOB_GENERIC_HEADER) */
struct upl_hob_header {
	/** HOB type */
	uint16_t type;
	/** HOB length (including this header) */
	uint16_t len;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/* HOB types */
#define UPL_HOB_HANDOFF			0x0001	/**< Phase handoff table */
#define UPL_HOB_MEMORY_ALLOCATION	0x0002	/**< Memory allocation */
#define UPL_HOB_RESOURCE_DESCRIPTOR	0x0003	/**< Resource descriptor */
#define UPL_HOB_GUID_EXTENSION		0x0004	/**< GUID extension */
#define UPL_HOB_END			0xffff	/**< End of HOB list */

/** Phase handoff information table HOB (EFI_HOB_HANDOFF_INFO_TABLE) */
struct upl_handoff_hob {
	/** Generic header */
	struct upl_hob_header hdr;
	/** Version */
	uint32_t version;
	/** Boot mode */
	uint32_t boot_mode;
	/** Highest address of memory allocated by the HOB producer */
	uint64_t memory_top;
	/** Lowest address of memory allocated by the HOB producer */
	uint64_t memory_bottom;
	/** Highest address of free memory */
	uint64_t free_memory_top;
	/** Lowest address of free memory */
	uint64_t free_memory_bottom;
	/** End of HOB list */
	uint64_t end_of_hob_list;
} __attribute__ (( packed ));

/** Resource descriptor HOB (EFI_HOB_RESOURCE_DESCRIPTOR) */
struct upl_resource_hob {
	/** Generic header */
	struct upl_hob_header hdr;
	/** Owner */
	struct upl_guid owner;
	/** Resource type */
	uint32_t type;
	/** Resource attributes */
	uint32_t attributes;
	/** Physical start address */
	uint64_t start;
	/** Length */
	uint64_t len;
} __attribute__ (( packed ));

/* Resource types */
#define UPL_RESOURCE_SYSTEM_MEMORY	0x00000000
#define UPL_RESOURCE_MEMORY_RESERVED	0x00000005

/** Memory allocation HOB (EFI_HOB_MEMORY_ALLOCATION) */
struct upl_memalloc_hob {
	/** Generic header */
	struct upl_hob_header hdr;
	/** Allocation name */
	struct upl_guid name;
	/** Base address */
	uint64_t base;
	/** Length */
	uint64_t len;
	/** Memory type (EFI_MEMORY_TYPE) */
	uint32_t type;
	/** Reserved */
	uint8_t reserved[4];
} __attribute__ (( packed ));

/* Memory types */
#define UPL_MEMORY_RESERVED		0	/**< EfiReservedMemoryType */
#define UPL_MEMORY_CONVENTIONAL		7	/**< EfiConventionalMemory */
#define UPL_MEMORY_UNUSABLE		8	/**< EfiUnusableMemory */
#define UPL_MEMORY_ACPI_RECLAIM		9	/**< EfiACPIReclaimMemory */
#define UPL_MEMORY_ACPI_NVS		10	/**< EfiACPIMemoryNVS */

/** A GUID extension HOB (EFI_HOB_GUID_TYPE) */
struct upl_guid_hob {
	/** Generic header */
	struct upl_hob_header hdr;
	/** GUID */
	struct upl_guid name;
} __attribute__ (( packed ));

/** Universal Payload generic header */
struct upl_header {
	/** Revision */
	uint8_t revision;
	/** Reserved */
	uint8_t reserved;
	/** Length of meaningful data (including this header) */
	uint16_t len;
} __attribute__ (( packed ));

/** Serial port information GUID */
#define UPL_SERIAL_GUID							\
	UPL_GUID ( 0xaa7e190d, 0xbe21, 0x4409,				\
		   0x8e, 0x67, 0xa2, 0xcd, 0x0f, 0x61, 0xe1, 0x70 )

/** Serial port information */
struct upl_serial {
	/** Universal Payload header */
	struct upl_header hdr;
	/** Registers are memory-mapped */
	uint8_t mmio;
	/** Register stride (in bytes) */
	uint8_t stride;
	/** Baud rate (or zero for default) */
	uint32_t baud;
	/** Register base address */
	uint64_t base;
} __attribute__ (( packed ));

/** ACPI table GUID */
#define UPL_ACPI_GUID							\
	UPL_GUID ( 0x9f9a9506, 0x5597, 0x4515,				\
		   0xba, 0xb6, 0x8b, 0xcd, 0xe7, 0x84, 0xba, 0x87 )

/** ACPI table */
struct upl_acpi {
	/** Universal Payload header */
	struct upl_header hdr;
	/** RSDP physical address */
	uint64_t rsdp;
} __attribute__ (( packed ));

/** SMBIOS 2.x table GUID */
#define UPL_SMBIOS_GUID							\
	UPL_GUID ( 0x590a0d26, 0x06e5, 0x4d20,				\
		   0x8a, 0x82, 0x59, 0xea, 0x1b, 0x34, 0x98, 0x2d )

/** SMBIOS 3.x table GUID */
#define UPL_SMBIOS3_GUID						\
	UPL_GUID ( 0x92b7896c, 0x3362, 0x46ce,				\
		   0x99, 0xb3, 0x4f, 0x5e, 0x3c, 0x34, 0xeb, 0x42 )

/** SMBIOS table */
struct upl_smbios {
	/** Universal Payload header */
	struct upl_header hdr;
	/** SMBIOS entry point physical address */
	uint64_t entry;
} __attribute__ (( packed ));

/** Graphics information GUID (EFI_PEI_GRAPHICS_INFO_HOB_GUID) */
#define UPL_GRAPHICS_GUID						\
	UPL_GUID ( 0x39f62cce, 0x6825, 0x4669,				\
		   0xbb, 0x56, 0x54, 0x1a, 0xba, 0x75, 0x3a, 0x07 )

/** Graphics information (EFI_PEI_GRAPHICS_INFO_HOB) */
struct upl_graphics {
	/** Frame buffer base address */
	uint64_t base;
	/** Frame buffer size */
	uint32_t size;
	/** Mode information version */
	uint32_t version;
	/** Horizontal resolution */
	uint32_t width;
	/** Vertical resolution */
	uint32_t height;
	/** Pixel format */
	uint32_t format;
	/** Red mask (for UPL_PIXEL_BITMASK) */
	uint32_t red_mask;
	/** Green mask (for UPL_PIXEL_BITMASK) */
	uint32_t green_mask;
	/** Blue mask (for UPL_PIXEL_BITMASK) */
	uint32_t blue_mask;
	/** Reserved mask (for UPL_PIXEL_BITMASK) */
	uint32_t reserved_mask;
	/** Pixels per scan line */
	uint32_t stride;
} __attribute__ (( packed ));

/* Pixel formats (EFI_GRAPHICS_PIXEL_FORMAT) */
#define UPL_PIXEL_RGBX		0	/**< Red, green, blue, reserved */
#define UPL_PIXEL_BGRX		1	/**< Blue, green, red, reserved */
#define UPL_PIXEL_BITMASK	2	/**< Defined by bit masks */

/** First character present in console font */
#define UPL_FONT_FIRST 0x20

/** Number of characters present in console font */
#define UPL_FONT_COUNT ( 0x7f - UPL_FONT_FIRST )

/** Console font height (in pixels) */
#define UPL_FONT_HEIGHT 19

extern const uint8_t upl_font[UPL_FONT_COUNT][UPL_FONT_HEIGHT];

extern uint64_t upl_mapped;

extern struct fdt_header * upl_hob_fdt ( const void *hob_list );
extern int upl_fdt_find ( const char *prefix, const char *compatible,
			  unsigned int *offset );
extern int upl_fdt_reserved ( const char *compatible, unsigned int index,
			      uint64_t *start );
extern void upl_fdt_memory ( void ( * add ) ( void *ctx, uint64_t start,
					      uint64_t size,
					      enum upl_memory_type type ),
			     void *ctx );
extern int upl_framebuffer ( struct upl_framebuffer *fb );
extern physaddr_t upl_acpi_rsdp ( void );
extern __asmcall __attribute__ (( noreturn )) void
upl_jump_ia32 ( uint32_t entry, uint32_t params );

#endif /* _IPXE_UPL_H */
