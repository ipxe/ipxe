#ifndef _IPXE_UPL_H
#define _IPXE_UPL_H

/** @file
 *
 * Universal Payload (UPL) interface
 *
 * The bootloader passes a Flattened Device Tree describing the memory
 * map, serial port, ACPI tables, SMBIOS tables, and graphics frame
 * buffer.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

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

/** First character present in console font */
#define UPL_FONT_FIRST 0x20

/** Number of characters present in console font */
#define UPL_FONT_COUNT ( 0x7f - UPL_FONT_FIRST )

/** Console font height (in pixels) */
#define UPL_FONT_HEIGHT 19

extern const uint8_t upl_font[UPL_FONT_COUNT][UPL_FONT_HEIGHT];

extern uint64_t upl_mapped;

extern int upl_fdt_find ( const char *prefix, const char *compatible,
			  unsigned int *offset );
extern int upl_fdt_reserved ( const char *compatible, unsigned int index,
			      uint64_t *start );
extern int upl_framebuffer ( struct upl_framebuffer *fb );
extern physaddr_t upl_acpi_rsdp ( void );

#endif /* _IPXE_UPL_H */
