/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config/console.h>
#include <config/general.h>

/** @file
 *
 * BIOS-specific configuration options
 *
 */

PROVIDE_REQUIRING_SYMBOL();

/*
 * Drag in all requested console types
 *
 */

#ifdef CONSOLE_PCBIOS
REQUIRE_OBJECT ( bios_console );
#endif
#ifdef CONSOLE_VESAFB
REQUIRE_OBJECT ( vesafb );
#endif
#ifdef CONSOLE_FRAMEBUFFER
REQUIRE_OBJECT ( vesafb );
#endif
#ifdef CONSOLE_INT13
REQUIRE_OBJECT ( int13con );
#endif

/*
 * Drag in all requested image types
 *
 */

/* Allow IMAGE_LKRN to be a synonynm for IMAGE_BZIMAGE */
#ifdef IMAGE_LKRN
REQUIRE_OBJECT ( bzimage );
#endif
