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

/**
 * @file
 *
 * Universal Payload frame buffer console
 *
 * The bootloader leaves the display in a fixed linear frame buffer
 * mode, which it describes via the device tree.  We cannot change
 * mode, so the console always uses that mode.
 *
 */

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ipxe/ansicol.h>
#include <ipxe/fbcon.h>
#include <ipxe/console.h>
#include <ipxe/uaccess.h>
#include <ipxe/init.h>
#include <ipxe/upl.h>
#include <config/console.h>

/* Set default console usage if applicable */
#if ( defined ( CONSOLE_FRAMEBUFFER ) && ! defined ( CONSOLE_UPLFB ) )
#define CONSOLE_UPLFB CONSOLE_FRAMEBUFFER
#endif
#if ! ( defined ( CONSOLE_UPLFB ) && CONSOLE_EXPLICIT ( CONSOLE_UPLFB ) )
#undef CONSOLE_UPLFB
#define CONSOLE_UPLFB ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

/* Forward declaration */
struct console_driver uplfb_console __console_driver;

/** A Universal Payload frame buffer */
struct uplfb {
	/** Frame buffer console */
	struct fbcon fbcon;
	/** Physical start address */
	physaddr_t start;
	/** Pixel geometry */
	struct fbcon_geometry pixel;
	/** Colour mapping */
	struct fbcon_colour_map map;
	/** Font definition */
	struct fbcon_font font;
};

/** The Universal Payload frame buffer */
static struct uplfb uplfb;

/**
 * Get character glyph
 *
 * @v character		Unicode character
 * @ret glyph		Character glyph
 */
static const uint8_t * uplfb_glyph ( unsigned int character ) {

	/* Substitute '?' for any character not present in the font */
	if ( ( character < UPL_FONT_FIRST ) ||
	     ( character >= ( UPL_FONT_FIRST + UPL_FONT_COUNT ) ) ) {
		character = '?';
	}
	return upl_font[ character - UPL_FONT_FIRST ];
}

/**
 * Generate colour mapping for a single colour component
 *
 * @v mask		Mask value
 * @v scale		Scale value to fill in
 * @v lsb		LSB value to fill in
 * @ret rc		Return status code
 */
static int uplfb_colour_map_mask ( uint32_t mask, uint8_t *scale,
				   uint8_t *lsb ) {
	uint32_t check;

	/* Fill in LSB and scale */
	*lsb = ( mask ? ( ffs ( mask ) - 1 ) : 0 );
	*scale = ( mask ? ( 8 - ( fls ( mask ) - *lsb ) ) : 8 );

	/* Check that original mask was contiguous */
	check = ( ( 0xff >> *scale ) << *lsb );
	if ( check != mask )
		return -ENOTSUP;

	return 0;
}

/**
 * Generate colour mapping
 *
 * @v fb		Frame buffer
 * @v map		Colour mapping to fill in
 * @ret bpp		Number of bits per pixel, or negative error
 */
static int uplfb_colour_map ( const struct upl_framebuffer *fb,
			      struct fbcon_colour_map *map ) {
	uint8_t reserved_scale;
	uint8_t reserved_lsb;
	int rc;

	/* Map each colour component */
	if ( ( rc = uplfb_colour_map_mask ( fb->red_mask, &map->red_scale,
					    &map->red_lsb ) ) != 0 )
		return rc;
	if ( ( rc = uplfb_colour_map_mask ( fb->green_mask, &map->green_scale,
					    &map->green_lsb ) ) != 0 )
		return rc;
	if ( ( rc = uplfb_colour_map_mask ( fb->blue_mask, &map->blue_scale,
					    &map->blue_lsb ) ) != 0 )
		return rc;
	if ( ( rc = uplfb_colour_map_mask ( fb->reserved_mask,
					    &reserved_scale,
					    &reserved_lsb ) ) != 0 )
		return rc;

	/* Calculate total number of bits per pixel */
	return ( 32 - ( reserved_scale + map->red_scale + map->green_scale +
			map->blue_scale ) );
}

/**
 * Initialise Universal Payload frame buffer
 *
 * @v config		Console configuration
 * @ret rc		Return status code
 */
static int uplfb_init ( struct console_configuration *config ) {
	struct console_configuration fixed;
	struct upl_framebuffer fb;
	int bpp;
	int rc;

	/* Locate frame buffer */
	if ( ( rc = upl_framebuffer ( &fb ) ) != 0 ) {
		DBGC ( &uplfb, "UPLFB found no frame buffer\n" );
		return -ENODEV;
	}
	if ( ( fb.base + fb.size ) > upl_mapped ) {
		DBGC ( &uplfb, "UPLFB frame buffer %#08llx is inaccessible\n",
		       ( ( unsigned long long ) fb.base ) );
		return -ENOTSUP;
	}

	/* Populate colour map */
	bpp = uplfb_colour_map ( &fb, &uplfb.map );
	if ( bpp < 0 ) {
		rc = bpp;
		return rc;
	}

	/* Populate pixel geometry */
	uplfb.pixel.width = fb.width;
	uplfb.pixel.height = fb.height;
	uplfb.pixel.len = ( ( bpp + 7 ) / 8 );
	uplfb.pixel.stride = fb.stride;
	uplfb.start = fb.base;
	DBGC ( &uplfb, "UPLFB using %dx%d %dbpp at %#08lx\n",
	       uplfb.pixel.width, uplfb.pixel.height, bpp, uplfb.start );

	/* Populate font */
	uplfb.font.height = UPL_FONT_HEIGHT;
	uplfb.font.glyph = uplfb_glyph;

	/* Use the fixed mode, treating any requested size as an upper bound */
	memcpy ( &fixed, config, sizeof ( fixed ) );
	if ( ( ! fixed.width ) || ( fixed.width > uplfb.pixel.width ) )
		fixed.width = uplfb.pixel.width;
	if ( ( ! fixed.height ) || ( fixed.height > uplfb.pixel.height ) )
		fixed.height = uplfb.pixel.height;
	fixed.depth = 0;

	/* Initialise frame buffer console */
	if ( ( rc = fbcon_init ( &uplfb.fbcon, phys_to_virt ( uplfb.start ),
				 &uplfb.pixel, &uplfb.map, &uplfb.font,
				 &fixed ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Print a character to current cursor position
 *
 * @v character		Character
 */
static void uplfb_putchar ( int character ) {

	fbcon_putchar ( &uplfb.fbcon, character );
}

/**
 * Configure console
 *
 * @v config		Console configuration, or NULL to use defaults
 * @ret rc		Return status code
 */
static int uplfb_configure ( struct console_configuration *config ) {
	static struct console_configuration defaults;
	int rc;

	/* Reset console, if applicable */
	if ( ! uplfb_console.disabled ) {
		fbcon_fini ( &uplfb.fbcon );
		ansicol_reset_magic();
	}
	uplfb_console.disabled = CONSOLE_DISABLED;

	/* Initialise frame buffer (full screen unless otherwise specified) */
	if ( ! config )
		config = &defaults;
	if ( ( rc = uplfb_init ( config ) ) != 0 )
		return rc;

	/* Mark console as enabled */
	uplfb_console.disabled = 0;

	/* Set magic colour to transparent if we have a background picture */
	if ( config->pixbuf )
		ansicol_set_magic_transparent();

	return 0;
}

/** Universal Payload frame buffer console driver */
struct console_driver uplfb_console __console_driver = {
	.usage = CONSOLE_UPLFB,
	.putchar = uplfb_putchar,
	.configure = uplfb_configure,
	.disabled = CONSOLE_DISABLED,
};

/**
 * Initialise frame buffer console
 *
 */
static void uplfb_init_console ( void ) {

	/* Failure is not fatal; the serial console remains available */
	uplfb_configure ( NULL );
}

/**
 * Shut down frame buffer console
 *
 * @v booting		System is shutting down in order to boot an OS
 */
static void uplfb_shutdown ( int booting __unused ) {

	/* Stop drawing, since the text cell array lives in the
	 * external heap, which may be overwritten when booting an OS.
	 */
	if ( ! uplfb_console.disabled ) {
		fbcon_fini ( &uplfb.fbcon );
		uplfb_console.disabled = CONSOLE_DISABLED;
	}
}

/** Frame buffer console initialisation function */
struct init_fn uplfb_init_fn __init_fn ( INIT_CONSOLE ) = {
	.name = "uplfb",
	.initialise = uplfb_init_console,
};

/** Frame buffer console startup function */
struct startup_fn uplfb_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "uplfb",
	.shutdown = uplfb_shutdown,
};
