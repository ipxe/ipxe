/* Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <realmode.h>
#include <biosint.h>
#include <basemem.h>
#include <gpxe/init.h>
#include <gpxe/hidemem.h>

/** Alignment for hidden memory regions */
#define ALIGN_HIDDEN 4096   /* 4kB page alignment should be enough */

/**
 * A hidden region of Etherboot
 *
 * This represents a region that will be edited out of the system's
 * memory map.
 *
 * This structure is accessed by assembly code, so must not be
 * changed.
 */
struct hidden_region {
	/* Physical start address */
	physaddr_t start;
	/* Physical end address */
	physaddr_t end;
};

/**
 * List of hidden regions
 *
 * Must be terminated by a zero entry.
 */
struct hidden_region __data16_array ( hidden_regions, [] ) = {
	[TEXT] = { 0, 0 },
	[BASEMEM] = { ( 640 * 1024 ), ( 640 * 1024 ) },
	[EXTMEM] = { 0, 0 },
	{ 0, 0, } /* Terminator */
};
#define hidden_regions __use_data16 ( hidden_regions )

/** Assembly routine in e820mangler.S */
extern void int15();

/** Vector for storing original INT 15 handler */
extern struct segoff __text16 ( int15_vector );
#define int15_vector __use_text16 ( int15_vector )

/**
 * Hide region of memory from system memory map
 *
 * @v start		Start of region
 * @v end		End of region
 */
void hide_region ( unsigned int region_id, physaddr_t start, physaddr_t end ) {
	struct hidden_region *region = &hidden_regions[region_id];

	/* Some operating systems get a nasty shock if a region of the
	 * E820 map seems to start on a non-page boundary.  Make life
	 * safer by rounding out our edited region.
	 */
	region->start = ( start & ~( ALIGN_HIDDEN - 1 ) );
	region->end = ( ( end + ALIGN_HIDDEN - 1 ) & ~( ALIGN_HIDDEN - 1 ) );

	DBG ( "Hiding region %d [%lx,%lx)\n",
	      region_id, region->start, region->end );
}

/**
 * Hide Etherboot text
 *
 */
static void hide_text ( void ) {

	/* The linker defines these symbols for us */
	extern char _text[];
	extern char _end[];

	hide_region ( TEXT, virt_to_phys ( _text ), virt_to_phys ( _end ) );
}

/**
 * Hide used base memory
 *
 */
void hide_basemem ( void ) {
	/* Hide from the top of free base memory to 640kB.  Don't use
	 * hide_region(), because we don't want this rounded to the
	 * nearest page boundary.
	 */
	hidden_regions[BASEMEM].start = ( get_fbms() * 1024 );
}

/**
 * Hide Etherboot
 *
 * Installs an INT 15 handler to edit Etherboot out of the memory map
 * returned by the BIOS.
 */
static void hide_etherboot ( void ) {

	/* Initialise the hidden regions */
	hide_text();
	hide_basemem();

	/* Hook INT 15 */
	hook_bios_interrupt ( 0x15, ( unsigned int ) int15,
			      &int15_vector );
}

/**
 * Unhide Etherboot
 *
 * Uninstalls the INT 15 handler installed by hide_etherboot(), if
 * possible.
 */
static void unhide_etherboot ( int flags __unused ) {

	/* If we have more than one hooked interrupt at this point, it
	 * means that some other vector is still hooked, in which case
	 * we can't safely unhook INT 15 because we need to keep our
	 * memory protected.  (We expect there to be at least one
	 * hooked interrupt, because INT 15 itself is still hooked).
	 */
	if ( hooked_bios_interrupts > 1 ) {
		DBG ( "Cannot unhide: %d interrupt vectors still hooked\n",
		      hooked_bios_interrupts );
		return;
	}

	/* Try to unhook INT 15.  If it fails, then just leave it
	 * hooked; it takes care of protecting itself.  :)
	 */
	unhook_bios_interrupt ( 0x15, ( unsigned int ) int15,
				&int15_vector );
}

/** Hide Etherboot startup function */
struct startup_fn hide_etherboot_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.startup = hide_etherboot,
	.shutdown = unhide_etherboot,
};
