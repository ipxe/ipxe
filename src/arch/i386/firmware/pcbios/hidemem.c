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
#include <gpxe/hidemem.h>

/* Linker-defined symbols */
extern char _text[];
extern char _end[];

/** Assembly routine in e820mangler.S */
extern void int15();

/** Vector for storing original INT 15 handler */
extern struct segoff __text16 ( int15_vector );
#define int15_vector __use_text16 ( int15_vector )

/**
 * List of hidden regions
 *
 * Must be terminated by a zero entry.
 */
struct hidden_region __data16_array ( hidden_regions, [] ) = {
	[TEXT] = { 0, 0 },
	[BASEMEM] = { 0, ( 640 * 1024 ) },
	[EXTMEM] = { 0, 0 },
	{ 0, 0, } /* Terminator */
};

/**
 * Hide Etherboot
 *
 * Installs an INT 15 handler to edit Etherboot out of the memory map
 * returned by the BIOS.
 */
void hide_etherboot ( void ) {
	hidden_regions[TEXT].start = virt_to_phys ( _text );
	hidden_regions[TEXT].end = virt_to_phys ( _end );
	hidden_regions[BASEMEM].start = ( rm_cs << 4 );

	DBG ( "Hiding [%lx,%lx) and [%lx,%lx)\n",
	      ( unsigned long ) hidden_regions[TEXT].start,
	      ( unsigned long ) hidden_regions[TEXT].end,
	      ( unsigned long ) hidden_regions[BASEMEM].start,
	      ( unsigned long ) hidden_regions[BASEMEM].end );

	hook_bios_interrupt ( 0x15, ( unsigned int ) int15,
			      &int15_vector );
}

/**
 * Unhide Etherboot
 *
 * Uninstalls the INT 15 handler installed by hide_etherboot(), if
 * possible.
 */
void unhide_etherboot ( void ) {
	unhook_bios_interrupt ( 0x15, ( unsigned int ) int15,
				&int15_vector );
}
